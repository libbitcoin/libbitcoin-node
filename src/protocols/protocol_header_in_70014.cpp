/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_header_in_70014.hpp>

#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_70014

using namespace system;
using namespace network;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in_70014::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(compact_block, handle_receive_compact_block, _1, _2);
    SUBSCRIBE_CHANNEL(compact_transactions,
        handle_receive_compact_transactions, _1, _2);

    protocol_header_in_70012::start();
}

// Compact block announcements are requested once current (high bandwidth).
void protocol_header_in_70014::complete() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!compact_ && is_current_chain(true))
    {
        compact_ = true;
        SEND((send_compact{ true, send_compact::compact_version_2 }),
            handle_send, _1);
        LOGP("Subscribed to compact block announcements at [" << opposite()
            << "].");
    }

    protocol_header_in_70012::complete();
}

// Inbound (cmpctblock).
// ----------------------------------------------------------------------------

bool protocol_header_in_70014::handle_receive_compact_block(const code& ec,
    const compact_block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    resolve();
    if (stopped())
        return false;

    // A compact block only extends a current chain, otherwise it is fetched.
    if (!is_current_chain(true))
        return true;

    const auto& header = message->header_ptr;
    const auto& hash = header->get_hash();
    set_announced(hash);

    if (fill_ && (fill_->hash == hash))
        return true;

    const auto link = archive().to_header(hash);
    if (link.is_terminal())
    {
        organize_compact(header, BIND(handle_organize_compact, _1, _2,
            message));
        return true;
    }

    collect(*message, link);
    return true;
}

// not stranded
void protocol_header_in_70014::handle_organize_compact(const code& ec,
    size_t, const compact_block::cptr& message) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    POST(do_organize_compact, ec, message);
}

void protocol_header_in_70014::do_organize_compact(const code& ec,
    const compact_block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto& hash = message->header_ptr->get_hash();
    if (ec && (ec != error::duplicate_header))
    {
        // An unstored parent is resolved by header synchronization.
        if (ec == error::orphan_header)
        {
            LOGP("Compact block [" << encode_hash(hash) << "] from ["
                << opposite() << "] " << ec.message());
            return;
        }

        LOGR("Compact block [" << encode_hash(hash) << "] from ["
            << opposite() << "] " << ec.message());
        stop(ec);
        return;
    }

    // The header of a weak branch is not stored.
    const auto link = archive().to_header(hash);
    if (!link.is_terminal())
        collect(*message, link);
}

// Collect (short ids).
// ----------------------------------------------------------------------------

void protocol_header_in_70014::collect(const compact_block& message,
    const database::header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Fill only a single block extension of the confirmed chain.
    if (query.is_associated(link) ||
        (query.to_candidate(query.get_top_candidate()) != link) ||
        (query.to_confirmed(query.get_top_confirmed()) !=
            query.to_parent(link)))
        return;

    chain::context ctx{};
    if (!query.get_context(ctx, link))
    {
        stop(fault(error::protocol2));
        return;
    }

    const auto& items = message.transactions;
    const auto& ids = message.short_ids;
    const auto count = ids.size() + items.size();
    if (is_zero(count) || (count > chain::max_block_weight))
    {
        stop(network::error::protocol_violation);
        return;
    }

    fill block{};
    block.link = link;
    block.height = ctx.height;
    block.header = message.header_ptr;
    block.hash = message.header_ptr->get_hash();
    block.key = to_compact_key(*message.header_ptr, message.nonce);
    block.short_ids.resize(count);
    block.txs.resize(count);
    block.links.assign(count, database::tx_link::terminal);

    // Prefilled indexes are differentially encoded (bip152).
    size_t position{};
    for (size_t item{}; item < items.size(); ++item)
    {
        const auto& prefilled = items.at(item);
        const auto offset = limit<size_t>(prefilled.index);
        position = is_zero(item) ? offset :
            ceilinged_add(add1(position), offset);

        if ((position >= count) || !prefilled.transaction_ptr)
        {
            stop(network::error::protocol_violation);
            return;
        }

        block.txs.at(position) = prefilled.transaction_ptr;
        block.unpooled.push_back(position);
    }

    // Short ids occupy the remaining positions in order.
    std::vector<uint64_t> short_ids{};
    std::vector<size_t> positions{};
    short_ids.reserve(ids.size());
    positions.reserve(ids.size());
    auto id = ids.begin();
    for (position = zero; position < count; ++position)
    {
        if (block.txs.at(position))
            continue;

        const auto value = from_mini(*id++);
        block.short_ids.at(position) = value;
        short_ids.push_back(value);
        positions.push_back(position);
    }

    database::tx_links links{};
    if (query.get_compact_links(links, short_ids, block.key))
    {
        stop(fault(error::protocol2));
        return;
    }

    for (size_t index{}; index < positions.size(); ++index)
    {
        position = positions.at(index);
        if (links.at(index) == database::tx_link::terminal)
        {
            block.missing.push_back(position);
            block.unpooled.push_back(position);
        }
        else
        {
            block.links.at(position) = links.at(index);
        }
    }

    std::sort(block.unpooled.begin(), block.unpooled.end());
    fill_.emplace(std::move(block));
    if (fill_->missing.empty())
    {
        identify();
        return;
    }

    // Requested indexes are differentially encoded (bip152).
    get_compact_transactions request{ fill_->hash, {} };
    request.indexes.reserve(fill_->missing.size());
    size_t previous{};
    for (size_t index{}; index < fill_->missing.size(); ++index)
    {
        position = fill_->missing.at(index);
        request.indexes.push_back(is_zero(index) ? position :
            position - add1(previous));
        previous = position;
    }

    SEND(std::move(request), handle_send, _1);
}

// Inbound (blocktxn).
// ----------------------------------------------------------------------------

bool protocol_header_in_70014::handle_receive_compact_transactions(
    const code& ec, const compact_transactions::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!fill_ || fill_->missing.empty() ||
        (message->block_hash != fill_->hash))
    {
        LOGP("Unrequested compact transactions from [" << opposite() << "].");
        return true;
    }

    auto& block = *fill_;
    const auto& txs = message->transaction_ptrs;
    if (txs.size() != block.missing.size())
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // The peer's short id for each position must match its own tx. A coinbase
    // short id is over its witness serialization, not its null wtxid (bip152).
    for (size_t index{}; index < txs.size(); ++index)
    {
        const auto& tx = txs.at(index);
        const auto position = block.missing.at(index);
        if (!tx || (to_short_id(block.key, tx->is_coinbase() ?
            bitcoin_hash(tx->to_data(true)) : tx->hash(true)) !=
            block.short_ids.at(position)))
        {
            LOGR("Invalid compact transaction from [" << opposite() << "].");
            stop(network::error::protocol_violation);
            return false;
        }

        block.txs.at(position) = tx;
    }

    block.missing.clear();
    identify();
    return true;
}

// Identify and submit.
// ----------------------------------------------------------------------------

void protocol_header_in_70014::identify() NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    auto& block = *fill_;
    const auto count = block.txs.size();

    // The coinbase is not pooled, so it is always in hand.
    const auto& first = block.txs.front();
    if (!first)
    {
        fill_.reset();
        stop(fault(error::protocol2));
        return;
    }

    hashes txids(count), wtxids(count);
    for (size_t position{}; position < count; ++position)
    {
        if (const auto& tx = block.txs.at(position); tx)
        {
            txids.at(position) = tx->hash(false);
            wtxids.at(position) = tx->hash(true);
            continue;
        }

        const auto& link = block.links.at(position);
        txids.at(position) = query.get_tx_key(link);
        wtxids.at(position) = query.get_wtxid(link);
        if ((txids.at(position) == null_hash) ||
            (wtxids.at(position) == null_hash))
        {
            fill_.reset();
            stop(fault(error::protocol2));
            return;
        }
    }

    // Malleated64 is a non-coinbase first tx and all txs of 64 bytes.
    auto malleated64 = !first->is_coinbase();
    for (size_t position{}; malleated64 && position < count; ++position)
    {
        size_t light{}, heavy{};
        const auto& tx = block.txs.at(position);
        malleated64 = tx ? (tx->serialized_size(false) == two * hash_size) :
            (query.get_tx_sizes(light, heavy, block.links.at(position)) &&
                (light == two * hash_size));
    }

    const auto segregated = !std::equal(txids.begin(), txids.end(),
        wtxids.begin());

    chain::context ctx{};
    if (!query.get_context(ctx, block.link))
    {
        fill_.reset();
        stop(fault(error::protocol2));
        return;
    }

    // An unidentified block leaves evidence, resolved when the block archives.
    code ec{};
    const auto& root = block.header->merkle_root();
    if ((ec = chain::block::identify(root, txids, malleated64)) ||
        (ec = chain::block::identify(ctx, *first, wtxids, segregated)))
    {
        LOGR("Compact block [" << encode_hash(block.hash) << "] from ["
            << opposite() << "] " << ec.message());

        evidence_link_ = block.link;
        evidence_ = std::move(wtxids);
        fill_.reset();
        return;
    }

    const auto unpooled = std::make_shared<chain::transaction_cptrs>();
    unpooled->reserve(block.unpooled.size());
    for (const auto position: block.unpooled)
        unpooled->push_back(block.txs.at(position));

    submit_compact(unpooled, block.links, block.link,
        BIND(handle_submit_compact, _1, _2, block.hash, block.height));
    fill_.reset();
}

// not stranded
void protocol_header_in_70014::handle_submit_compact(const code& ec, size_t,
    const hash_digest& hash, size_t height) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    POST(do_submit_compact, ec, hash, height);
}

void protocol_header_in_70014::do_submit_compact(const code& ec,
    const hash_digest& hash, size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // Another channel may have completed the block.
    if (ec == error::duplicate_block)
        return;

    // The tx chaser marks an identified block with an invalid tx unconfirmable.
    if (ec)
    {
        LOGR("Compact block [" << encode_hash(hash) << ":" << height
            << "] from [" << opposite() << "] " << ec.message());
        stop(ec);
        return;
    }

    LOGP("Compact block [" << encode_hash(hash) << ":" << height
        << "] from [" << opposite() << "].");
}

// Evidence.
// ----------------------------------------------------------------------------

// Evidence of an unidentified compact block is held until the peer's next
// compact block, when it is resolved against the block if then archived.
void protocol_header_in_70014::resolve() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (evidence_.empty())
        return;

    const auto& query = archive();
    if (query.is_associated(evidence_link_) &&
        (query.get_wtxids(evidence_link_) != evidence_))
    {
        LOGR("Invalid compact block ["
            << encode_hash(query.get_header_key(evidence_link_))
            << "] from [" << opposite() << "].");
        stop(network::error::protocol_violation);
    }

    evidence_link_ = {};
    evidence_.clear();
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
