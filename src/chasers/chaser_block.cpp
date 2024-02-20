/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser_block.hpp>

#include <functional>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {
    
using namespace network;
using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_block::chaser_block(full_node& node) NOEXCEPT
  : chaser(node),
    checkpoints_(node.config().bitcoin.checkpoints),
    currency_window_(node.node_settings().currency_window()),
    use_currency_window_(to_bool(node.node_settings().currency_window_minutes))
{
}

chaser_block::~chaser_block() NOEXCEPT
{
}

// protected
const network::wall_clock::duration&
chaser_block::currency_window() const NOEXCEPT
{
    return currency_window_;
}

// protected
bool chaser_block::use_currency_window() const NOEXCEPT
{
    return use_currency_window_;
}

// protected
code chaser_block::start() NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser_block");

    state_ = archive().get_candidate_chain_state(config().bitcoin);
    BC_ASSERT_MSG(state_, "Store not initialized.");

    return subscribe(std::bind(&chaser_block::handle_event,
        this, _1, _2, _3));
}

// protected
void chaser_block::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_block::do_handle_event,
            this, ec, event_, value));
}

// private
void chaser_block::do_handle_event(const code&, chase, link) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_block");
}

void chaser_block::organize(const chain::block::cptr& block) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_block::do_organize,
            this, block));
}

// private
void chaser_block::do_organize(const chain::block::cptr& block_ptr) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_block");

    auto& query = archive();
    const auto& block = *block_ptr;
    const auto& header = block.header();
    const auto& previous = header.previous_block_hash();
    const auto& coin = config().bitcoin;
    const auto hash = header.hash();

    // Skip existing, fail orphan.
    // ------------------------------------------------------------------------

    // Block (header and txs) already exists.
    if (tree_.contains(hash) || query.is_block(hash))
        return;

    // Peer processing should have precluded orphan submission.
    if (!tree_.contains(previous) && !query.is_block(previous))
    {
        stop(error::orphan_block);
        return;
    }

    // Validate block.
    // ------------------------------------------------------------------------

    // Rolling forward chain_state eliminates requery cost.
    // Do not use block ref here as the block override is for tx pool.
    state_.reset(new chain::chain_state(*state_, header, coin));
    const auto context = state_->context();

    // Checkpoints are considered chain not block/header validation.
    if (chain::checkpoint::is_conflict(coin.checkpoints, hash,
        state_->height()))
    {
        ////LOGR("Invalid block (checkpoint) [" << encode_hash(hash)
        ////    << "] from [" << authority() << "].");
        ////stop(network::error::protocol_violation);
        ////return false;
    };

    // Block validations are bypassed when under checkpoint/milestone.
    if (!chain::checkpoint::is_under(coin.checkpoints, state_->height()))
    {
        auto error = block.check();
        if (error)
        {
            ////LOGR("Invalid block (check) [" << encode_hash(hash)
            ////    << "] from [" << authority() << "] " << error.message());
            ////stop(network::error::protocol_violation);
            ////return false;
        }

        error = block.check(context);
        if (error)
        {
            ////LOGR("Invalid block (check(context)) [" << encode_hash(hash)
            ////    << "] from [" << authority() << "] " << error.message());
            ////stop(network::error::protocol_violation);
            ////return false;
        }

        // Populate prevouts only, internal to block.
        // ********************************************************************
        // TODO: populate input metadata for block internal.
        // ********************************************************************
        ////block.populate();

        // ********************************************************************
        // TODO: populate prevouts and input metadata for block tree.
        // ********************************************************************

        // Populate stored missing prevouts only, not input metadata.
        // ********************************************************************
        // TODO: populate input metadata for stored blocks.
        // ********************************************************************
        ////auto& query = archive();
        ////if (!query.populate(block))
        ////{
        ////    ////LOGR("Invalid block (populate) [" << encode_hash(hash)
        ////    ////    << "] from [" << authority() << "].");
        ////    ////stop(network::error::protocol_violation);
        ////    ////return false;
        ////}

        ////// TODO: also requires input metadata population.
        ////error = block.accept(context, coin.subsidy_interval_blocks,
        ////    coin.initial_subsidy());
        ////if (error)
        ////{
        ////    ////LOGR("Invalid block (accept) [" << encode_hash(hash)
        ////    ////    << "] from [" << authority() << "] " << error.message());
        ////    ////stop(network::error::protocol_violation);
        ////    ////return false;
        ////}

        ////// Requires only prevout population.
        ////error = block.connect(context);
        ////if (error)
        ////{
        ////    ////LOGR("Invalid block (connect) [" << encode_hash(hash)
        ////    ////    << "] from [" << authority() << "] " << error.message());
        ////    ////stop(network::error::protocol_violation);
        ////    ////return false;
        ////}

        // ********************************************************************
        // TODO: with all metadata populated, block.confirm may be possible.
        // ********************************************************************
    }

    // Compute relative work.
    // ------------------------------------------------------------------------

    // Block is new top of stale branch (strength not computed).
    if (!is_current(header, context.height))
    {
        save(block_ptr, context);
        return;
    }

    size_t point{};
    uint256_t work{};
    hashes tree_branch{};
    header_links store_branch{};
    if (!get_branch_work(work, point, tree_branch, store_branch, header))
    {
        stop(error::store_integrity);
        return;
    }

    bool strong{};
    if (!get_is_strong(strong, work, point))
    {
        stop(error::store_integrity);
        return;
    }

    // If a long candidate chain is first created using headers-first and then
    // blocks-first is executed (after a restart/config) it can result in up to
    // the entire blockchain being cached into memory before becoming strong,
    // which means stronger than the candidate chain. While switching config
    // between modes by varying network protocol is supported, blocks-first is
    // inherently inefficient and weak on this aspect of DoS protection. This
    // is acceptable for its purpose and consistent with early implementations.
    if (!strong)
    {
        // Block is new top of current weak branch.
        save(block_ptr, context);
        return;
    }

    // Reorganize candidate chain.
    // ------------------------------------------------------------------------

    // Obtain the top height.
    auto top = query.get_top_candidate();
    if (top < point)
    {
        stop(error::store_integrity);
        return;
    }

    // Pop down to the branch point, underflow guarded above.
    while (top-- > point)
    {
        if (!query.pop_candidate())
        {
            stop(error::store_integrity);
            return;
        }
    }

    // Push stored strong block headers to candidate chain.
    for (const auto& link: views_reverse(store_branch))
    {
        if (!query.push_candidate(link))
        {
            stop(error::store_integrity);
            return;
        }
    }

    // Store strong tree blocks and push headers to candidate chain.
    for (const auto& key: views_reverse(tree_branch))
    {
        if (!push(key))
        {
            stop(error::store_integrity);
            return;
        }
    }

    // Push new block as top of candidate chain.
    const auto link = push(block_ptr, context);
    if (link.is_terminal())
    {
        stop(error::store_integrity);
        return;
    }

    // Notify candidate reorganization with branch point.
    // ------------------------------------------------------------------------

    notify(error::success, chase::block,
        { possible_narrow_cast<height_t>(point) });
}

// protected
bool chaser_block::is_current(const chain::header& header,
    size_t height) const NOEXCEPT
{
    if (!use_currency_window())
        return true;

    // Checkpoints are already validated. Current if at a checkpoint height.
    if (chain::checkpoint::is_at(checkpoints_, height))
        return true;

    // en.wikipedia.org/wiki/Time_formatting_and_storage_bugs#Year_2106
    const auto time = wall_clock::from_time_t(header.timestamp());
    const auto current = wall_clock::now() - currency_window();
    return time >= current;
}

// protected
bool chaser_block::get_branch_work(uint256_t& work, size_t& point,
    system::hashes& tree_branch, header_links& store_branch,
    const chain::header& header) const NOEXCEPT
{
    // Use pointer to avoid const/copy.
    auto previous = &header.previous_block_hash();
    tree_branch.clear();
    store_branch.clear();
    work = header.proof();
    const auto& query = archive();

    // Sum all branch work from tree.
    for (auto it = tree_.find(*previous); it != tree_.end();
        it = tree_.find(*previous))
    {
        previous = &it->second.item->header().previous_block_hash();
        tree_branch.push_back(it->second.item->header().hash());
        work += it->second.item->header().proof();
    }

    // Sum branch work from store.
    database::height_link link{};
    for (link = query.to_header(*previous); !query.is_candidate_block(link);
        link = query.to_parent(link))
    {
        uint32_t bits{};
        if (link.is_terminal() || !query.get_bits(bits, link))
            return false;

        store_branch.push_back(link);
        work += system::chain::header::proof(bits);
    }

    // TODO: this is always zero at new store.
    // TODO: this could be the result of is_candidate_block being false.
    // Height of the highest candidate header is the branch point.
    return query.get_height(point, link);
}

// protected
// ****************************************************************************
// CONSENSUS: branch with greater work causes candidate reorganization.
// Chasers eventually reorganize candidate branch into confirmed if valid.
// ****************************************************************************
bool chaser_block::get_is_strong(bool& strong, const uint256_t& work,
    size_t point) const NOEXCEPT
{
    strong = false;
    uint256_t candidate_work{};
    const auto& query = archive();
    for (auto height = query.get_top_candidate(); height > point; --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_candidate(height)))
            return false;

        candidate_work += chain::header::proof(bits);
        if (!((strong = work > candidate_work)))
            return true;
    }

    strong = true;
    return true;
}

// protected
void chaser_block::save(const chain::block::cptr& block,
    const chain::context& context) NOEXCEPT
{
    tree_.insert(
    {
        block->hash(),
        {
            {
                possible_narrow_cast<flags_t>(context.forks),
                possible_narrow_cast<height_t>(context.height),
                context.median_time_past,
            },
            block
        }
    });
}

// protected
database::header_link chaser_block::push(const chain::block::cptr& block,
    const chain::context& context) const NOEXCEPT
{
    auto& query = archive();
    const auto link = query.set_link(*block, database::context
        {
            possible_narrow_cast<flags_t>(context.forks),
            possible_narrow_cast<height_t>(context.height),
            context.median_time_past,
        });

    if (!query.push_candidate(link))
        return {};

    return link;
}

// protected
bool chaser_block::push(const system::hash_digest& key) NOEXCEPT
{
    const auto value = tree_.extract(key);
    BC_ASSERT_MSG(!value.empty(), "missing tree value");

    auto& query = archive();
    const auto& node = value.mapped();
    return query.push_candidate(query.set_link(*node.item, node.context));
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
