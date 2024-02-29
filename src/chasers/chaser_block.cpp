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

#include <algorithm>
#include <functional>
#include <utility>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_block

using namespace network;
using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_block::chaser_block(full_node& node) NOEXCEPT
  : chaser(node),
    checkpoints_(node.config().bitcoin.checkpoints)
{
}

chaser_block::~chaser_block() NOEXCEPT
{
}

// start
// ----------------------------------------------------------------------------

code chaser_block::start() NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser_block");

    // Initialize cache of top candidate chain state.
    top_state_ = archive().get_candidate_chain_state(
        config().bitcoin, archive().get_top_candidate());

    return SUBSCRIBE_EVENT(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_block::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::unconnected)
    {
        POST(handle_unconnected, std::get<height_t>(value));
    }
}

void chaser_block::handle_unconnected(height_t) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_block");
}

// methods
// ----------------------------------------------------------------------------

void chaser_block::organize(const block::cptr& block,
    organize_handler&& handler) NOEXCEPT
{
    POST(do_organize, block, std::move(handler));
}

void chaser_block::do_organize(const block::cptr& block_ptr,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_block");

    auto& query = archive();
    const auto& block = *block_ptr;
    const auto& header = block.header();
    const auto& coin = config().bitcoin;
    const auto hash = header.hash();

    // Skip existing/orphan, get state.
    // ------------------------------------------------------------------------

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (tree_.contains(hash))
    {
        handler(error::duplicate_block, {});
        return;
    }

    // If header exists test for prior invalidity as a block.
    const auto id = query.to_header(hash);
    if (!id.is_terminal())
    {
        const auto ec = query.get_block_state(id);
        if (ec == database::error::block_unconfirmable)
        {
            handler(ec, {});
            return;
        }

        if (ec != database::error::unassociated)
        {
            handler(error::duplicate_block, {});
            return;
        }
    }

    // Results from running headers-first and then blocks-first.
    auto state = get_state(header.previous_block_hash());
    if (!state)
    {
        handler(error::orphan_block, {});
        return;
    }

    // Roll chain state forward from previous to current header.
    // Do not use block parameter here as that override is for tx pool.
    state.reset(new chain_state{ *state, header, coin });
    const auto height = state->height();

    // Validate block.
    // ------------------------------------------------------------------------

    // Checkpoints are considered chain not block/header validation.
    if (checkpoint::is_conflict(coin.checkpoints, hash, height))
    {
        handler(system::error::checkpoint_conflict, height);
        return;
    };

    // Block validations are bypassed when under checkpoint/milestone.
    if (!checkpoint::is_under(coin.checkpoints, height))
    {
        // Requires no population.
        if (const auto error = block.check())
        {
            handler(error, height);
            return;
        }

        // Requires no population.
        if (const auto error = block.check(state->context()))
        {
            handler(error, height);
            return;
        }

        // Populate prevouts from self/tree and store.
        populate(block);
        if (!query.populate(block))
        {
            handler(network::error::protocol_violation, height);
            return;
        }

        // Requires only prevout population.
        if (const auto error = block.accept(state->context(),
            coin.subsidy_interval_blocks, coin.initial_subsidy()))
        {
            handler(error, height);
            return;
        }

        // Requires only prevout population.
        if (const auto error = block.connect(state->context()))
        {
            handler(error, height);
            return;
        }
    }

    // Compute relative work.
    // ------------------------------------------------------------------------
    // Current is not used for blocks due to excessive cache requirement.

    size_t point{};
    uint256_t work{};
    hashes tree_branch{};
    header_links store_branch{};
    if (!get_branch_work(work, point, tree_branch, store_branch, header))
    {
        handler(error::store_integrity, height);
        return;
    }

    bool strong{};
    if (!get_is_strong(strong, work, point))
    {
        handler(error::store_integrity, height);
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
        cache(block_ptr, state);
        handler(error::success, height);
        return;
    }

    // Reorganize candidate chain.
    // ------------------------------------------------------------------------

    auto top = top_state_->height();
    if (top < point)
    {
        handler(error::store_integrity, height);
        return;
    }

    // Pop down to the branch point.
    while (top-- > point)
    {
        if (!query.pop_candidate())
        {
            handler(error::store_integrity, height);
            return;
        }
    }

    // Push stored strong block headers to candidate chain.
    for (const auto& link: views_reverse(store_branch))
    {
        if (!query.push_candidate(link))
        {
            handler(error::store_integrity, height);
            return;
        }
    }

    // Store strong tree blocks and push headers to candidate chain.
    for (const auto& key: views_reverse(tree_branch))
    {
        if (!push(key))
        {
            handler(error::store_integrity, height);
            return;
        }
    }

    // Push new block as top of candidate chain.
    if (push(block_ptr, state->context()).is_terminal())
    {
        handler(error::store_integrity, height);
        return;
    }

    top_state_ = state;
    const auto branch_point = possible_narrow_cast<height_t>(point);
    notify(error::success, chase::block, { branch_point });
    handler(error::success, height);
}

// utilities
// ----------------------------------------------------------------------------

chain_state::ptr chaser_block::get_state(
    const hash_digest& hash) const NOEXCEPT
{
    if (!top_state_)
        return {};

    // Top state is cached because it is by far the most commonly retrieved.
    if (top_state_->hash() == hash)
        return top_state_;

    const auto it = tree_.find(hash);
    if (it != tree_.end())
        return it->second.state;

    size_t height{};
    const auto& query = archive();
    if (query.get_height(height, query.to_header(hash)))
        return query.get_candidate_chain_state(config().bitcoin, height);

    return {};
}

bool chaser_block::get_branch_work(uint256_t& work, size_t& point,
    hashes& tree_branch, header_links& store_branch,
    const header& header) const NOEXCEPT
{
    // Use pointer to avoid const/copy.
    auto previous = &header.previous_block_hash();
    const auto& query = archive();
    work = header.proof();

    // Sum all branch work from tree.
    for (auto it = tree_.find(*previous); it != tree_.end();
        it = tree_.find(*previous))
    {
        previous = &it->second.block->header().previous_block_hash();
        tree_branch.push_back(it->second.block->header().hash());
        work += it->second.block->header().proof();
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
        work += chain::header::proof(bits);
    }

    // Height of the highest candidate header is the branch point.
    return query.get_height(point, link);
}

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

    // Accumulate candidate branch and when exceeds branch return false (weak).
    for (auto height = query.get_top_candidate(); height > point; --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_candidate(height)))
            return false;

        candidate_work += header::proof(bits);
        if (candidate_work >= work)
            return true;
    }

    strong = true;
    return true;
}

void chaser_block::cache(const block::cptr& block,
    const chain_state::ptr& state) NOEXCEPT
{
    tree_.insert({ block->hash(), { block, state } });
}

database::header_link chaser_block::push(const block::cptr& block,
    const context& context) const NOEXCEPT
{
    auto& query = archive();
    const auto link = query.set_link(*block, database::context
    {
        possible_narrow_cast<flags_t>(context.forks),
        possible_narrow_cast<height_t>(context.height),
        context.median_time_past,
    });

    return query.push_candidate(link) ? link : database::header_link{};
}

bool chaser_block::push(const hash_digest& key) NOEXCEPT
{
    const auto value = tree_.extract(key);
    BC_ASSERT_MSG(!value.empty(), "missing tree value");

    auto& query = archive();
    const auto& node = value.mapped();
    const auto link = query.set_link(*node.block, node.state->context());
    return query.push_candidate(link);
}

void chaser_block::set_prevout(const input& input) const NOEXCEPT
{
    const auto& point = input.point();

    // Scan all blocks for matching tx (linear :/)
    std::for_each(tree_.begin(), tree_.end(), [&](const auto& element) NOEXCEPT
    {
        const auto& txs = *element.second.block->transactions_ptr();
        const auto it = std::find_if(txs.begin(), txs.end(),
            [&](const auto& tx) NOEXCEPT
            {
                return tx->hash(false) == point.hash();
            });

        if (it != txs.end())
        {
            const auto& tx = **it;
            const auto& outs = *tx.outputs_ptr();
            if (point.index() < outs.size())
            {
                input.prevout = outs.at(point.index());
                return;
            }
        }
    });
}

// metadata is mutable so can be set on a const object.
void chaser_block::populate(const block& block) const NOEXCEPT
{
    block.populate();
    const auto ins = block.inputs_ptr();
    std::for_each(ins->begin(), ins->end(), [&](const auto& in) NOEXCEPT
    {
        if (!in->prevout && !in->point().is_null())
            set_prevout(*in);
    });
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
