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
#include <bitcoin/node/chasers/chaser_transaction.hpp>

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_transaction

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// compact
// ----------------------------------------------------------------------------

void chaser_transaction::submit_compact(const transactions_cptr& txs,
    const database::tx_links& links, const database::header_link& link,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit_compact, txs, links, link, std::move(handler));
}

// A compact block is not filled while a block archives off the strand, so a
// block that finds the pool closed archives alone.
bool chaser_transaction::archive_block(code& ec,
    const chain::view::block::cptr& block, const database::header_link& link,
    bool strong, bool bypass, size_t height, bool prune,
    network::result_handler&& handler) NOEXCEPT
{
    ++writers_;
    if (pooling_)
    {
        --writers_;
        if (!closed())
            POST(do_archive_block, block, link, strong, bypass, height,
                prune, std::move(handler));

        return false;
    }

    auto& query = archive();
    ec = query.is_associated(link) ? error::duplicate_block :
        query.set_code(*block, link, strong, bypass, height, prune);

    --writers_;
    return true;
}

// private
void chaser_transaction::do_submit_compact(const transactions_cptr& txs,
    const database::tx_links& links, const database::header_link& link,
    const submit_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    const auto start = now();
    auto& query = archive();
    if (query.is_associated(link))
    {
        handler(error::duplicate_block, {});
        return;
    }

    // Not pooling, a block absorbs stored txs in its protocol, off the strand.
    if (!pooling_ || is_nonzero(writers_.load()))
    {
        handler(error::pooling_disabled, {});
        return;
    }

    chain::context ctx{};
    if (!query.get_context(ctx, link))
    {
        handler(fault(error::transaction3), {});
        return;
    }

    constexpr auto terminal = database::tx_link::terminal;
    const auto count = std::ranges::count(links, terminal);
    if (possible_narrow_sign_cast<size_t>(count) != txs->size())
    {
        handler(error::transaction5, {});
        return;
    }

    using link_t = database::tx_link::integer;
    const auto filler = [&](link_t& fk, size_t index) NOEXCEPT
    {
        database::tx_link out{};
        if (const auto ec = fill(out, *txs->at(index), ctx))
        {
            const auto faulted = node::error::error_category::contains(ec) ||
                database::error::error_category::contains(ec);
            handler(faulted ? ec : unconfirmable(link, ctx.height, ec), index);
            return false;
        }

        fk = out;
        return true;
    };

    // Txs are an arbitrary subset of the block, so block checks are deferred to
    // validation. Each is archived before the next, so in block order a parent
    // is resolved from the store. The coinbase (never a parent within its
    // block) is archived last, as validation indexes only txs linked above it.
    auto filled = links;
    const auto deferred = filled.front() == terminal;
    auto index = deferred ? one : zero;
    for (auto fk = std::next(filled.begin()); fk != filled.end(); ++fk)
    {
        if (*fk != terminal)
            continue;

        if (!filler(*fk, index))
            return;

        ++index;
    }

    if (deferred && !filler(filled.front(), zero))
        return;

    if (pooling_ && !sweep())
    {
        handler(fault(error::transaction11), {});
        return;
    }

    const auto strong = is_under_checkpoint(ctx.height);
    if (const auto ec = query.set_code(link, filled, strong))
    {
        handler(fault(ec), {});
        return;
    }

    fire_span(events::pool_compact_usecs, start);
    notify(error::success, chases::checked{ ctx.height });
    fire(events::block_archived, ctx.height);
    handler(error::success, {});
}

// private
void chaser_transaction::do_archive_block(
    const chain::view::block::cptr& block, const database::header_link& link,
    bool strong, bool bypass, size_t height, bool prune,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped);
        return;
    }

    auto& query = archive();
    if (query.is_associated(link))
    {
        handler(error::duplicate_block);
        return;
    }

    handler(query.set_code(*block, link, strong, bypass, height, prune));
}

// The stored tx, or validated and archived, pooled under the block context.
code chaser_transaction::fill(database::tx_link& out,
    const chain::transaction& tx, const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    out = to_stored(tx);
    if (!out.is_terminal())
    {
        if (tx.is_coinbase() || !query.is_pooling() || !reusable(ctx) ||
            query.is_pooled(out) || query.is_confirmed_tx(out))
            return error::success;

        if (const auto ec = validate(tx, ctx))
            return ec;

        const auto admit = now();
        const auto admitted = query.set_pooled(out, tx, ctx, ctx.timestamp);
        report(pool_scan::admission, admit);
        if (!admitted)
            return fault(error::transaction2);

        if (ctx.height >= query.silent_start_height() &&
            ctx.is_enabled(flags::bip341_rule) && !query.set_silent(out, tx))
            return fault(error::transaction6);

        due_ = due_ || !is_sufficient(ctx);
        notify(error::success, chases::transaction{ out });
        broadcast(tx);
        return error::success;
    }

    if (const auto ec = validate(tx, ctx))
        return ec;

    bool pooled{};

    // Disk full may leave txs partly archived, resolves by resubmit.
    if (const auto ec = query.set_code(out, pooled, tx))
        return fault(ec);

    const auto coinbase = tx.is_coinbase();
    if (!pooled && !coinbase)
    {
        if (reusable(ctx))
        {
            const auto admit = now();
            const auto admitted = query.set_pooled(out, tx, ctx,
                ctx.timestamp);

            report(pool_scan::admission, admit);
            if (!admitted)
                return fault(error::transaction2);

            due_ = due_ || !is_sufficient(ctx);
        }

        if (ctx.height >= query.silent_start_height() &&
            ctx.is_enabled(flags::bip341_rule) && !query.set_silent(out, tx))
            return fault(error::transaction6);
    }

    if (!coinbase)
    {
        fire(events::tx_archived, to_rate(tx));
        notify(error::success, chases::transaction{ out });
        broadcast(tx);
    }

    return error::success;
}

// An identified block with an invalid tx is invalid.
code chaser_transaction::unconfirmable(const database::header_link& link,
    size_t height, const code& ec) NOEXCEPT
{
    if (!archive().set_block_unconfirmable(link))
        return fault(error::transaction4);

    LOGR("Compact block failed check [" << height << "] " << ec.message());
    notify(error::success, chases::unchecked{ link });
    fire(events::block_unconfirmable, height);
    return ec;
}

// validation
// ----------------------------------------------------------------------------

// A block tx is under proof of work, so it is not subject to policy (fee
// rate, guards) and may conflict with unconfirmed txs.
code chaser_transaction::validate(const chain::transaction& tx,
    const chain::context& ctx) NOEXCEPT
{
    code ec{};
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(ctx);
    if (ec || tx.is_coinbase())
        return ec;

    archive().populate_with_metadata(tx);
    ec = tx.accept(ctx);
    if (!ec) ec = tx.connect(ctx);
    return ec;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
