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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_ORGANIZE_IPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_ORGANIZE_IPP

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_NEW_OR_DELETE)

// Public
// ----------------------------------------------------------------------------

TEMPLATE
CLASS::chaser_organize(full_node& node) NOEXCEPT
  : chaser(node),
    settings_(config().bitcoin)
{
}

TEMPLATE
code CLASS::start() NOEXCEPT
{
    using namespace system;
    using namespace std::placeholders;
    const auto& query = archive();

    // Initialize cache of top candidate chain state.
    // Spans full chain to obtain cumulative work. This can be optimized by
    // storing it with each header, though the scan is fast. The same occurs
    // when a block first branches below the current chain top. Chain work
    // is a questionable DoS protection scheme only, so could also toss it.
    state_ = query.get_candidate_chain_state(settings_,
        query.get_top_candidate());

    LOGN("Candidate top [" << encode_hash(state_->hash()) << ":"
        << state_->height() << "].");

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

TEMPLATE
void CLASS::organize(const typename Block::cptr& block_ptr,
    organize_handler&& handler) NOEXCEPT
{
    if (!closed())
    {
        POST(do_organize, block_ptr, std::move(handler));
    }
}

// Properties
// ----------------------------------------------------------------------------

TEMPLATE
const system::settings& CLASS::settings() const NOEXCEPT
{
    return settings_;
}

TEMPLATE
const typename CLASS::block_tree& CLASS::tree() const NOEXCEPT
{
    return tree_;
}

// Methods
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::handle_event(const code&, chase event_, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    using namespace system;
    switch (event_)
    {
        case chase::unchecked:
        case chase::unpreconfirmable:
        case chase::unconfirmable:
        {
            POST(do_disorganize, possible_narrow_cast<header_t>(value));
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

TEMPLATE
void CLASS::do_organize(typename Block::cptr& block_ptr,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace system;
    const auto& block = *block_ptr;
    const auto hash = block.hash();
    const auto header = get_header(block);
    auto& query = archive();

    // Skip existing/orphan, get state.
    // ........................................................................

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    const auto it = tree_.find(hash);
    if (it != tree_.end())
    {
        handler(error_duplicate(), it->second.state->height());
        return;
    }

    const auto id = query.to_header(hash);
    if (!id.is_terminal())
    {
        size_t height{};
        if (!query.get_height(height, id))
        {
            handler(suspend(error::store_integrity), {});
            return;
        }

        // block_unconfirmable is not set when merkle tree is malleable, in
        // which case the header may be archived in an undetermined state. Not
        // setting block_unconfirmable only delays ineviable invalidity 
        // discovery and consequential deorganization at that block. Though
        // this may cycle until a strong candidate chain is located.
        const auto ec = query.get_header_state(id);
        if (ec == database::error::block_unconfirmable)
        {
            // This eventually stops the peer, but the full set of headers may
            // still cycle through to become strong, despite this being stored
            // as block_unconfirmable from a block validate or confirm failure.
            // Block preconfirmation will then fail and this cycle will
            // continue until a strong candidate chain is located. The cycle
            // occurs because peers continue to send the same headers,which
            // may indicate an local failure or peer failures.
            handler(ec, height);
            return;
        }

        // With a candidate reorg that drop strong below a valid header chain,
        // this will cause a sequence of headers to be bypassed, such that a
        // parent of a block that doesn't exist will not be a candidate, which
        // result in a failure of get_chain_state below, because it depends on
        // candidate state. So get_chain_state needs to be chain independent.
        if (!is_block() || ec != database::error::unassociated)
        {
            handler(error_duplicate(), height);
            return;
        }
    }

    // Obtain header chain state.
    // ........................................................................

    // Obtain parent state from state_, tree, or store as applicable.
    const auto parent = get_chain_state(header.previous_block_hash());
    if (!parent)
    {
        handler(error_orphan(), {});
        return;
    }

    // Roll chain state forward from archived parent to current header.
    const auto state = std::make_shared<chain_state>(*parent, header, settings_);

    // Validation and currency.
    // ........................................................................

    const auto height = state->height();
    if (chain::checkpoint::is_conflict(settings_.checkpoints, hash, height))
    {
        handler(system::error::checkpoint_conflict, height);
        return;
    };

    if (const auto ec = validate(block, *state))
    {
        handler(ec, height);
        return;
    }

    // Store with checkpoint, milestone, or currency with sufficient work.
    if (!is_storable(block, *state))
    {
        log_state_change(*parent, *state);
        cache(block_ptr, state);
        handler(error::success, height);
        return;
    }

    // Compute relative work.
    // ........................................................................

    bool strong{};
    uint256_t work{};
    hashes tree_branch{};
    size_t branch_point{};
    header_links store_branch{};

    if (!get_branch_work(work, branch_point, tree_branch, store_branch, header) ||
        !get_is_strong(strong, work, branch_point))
    {
        handler(suspend(error::store_integrity), height);
        return;
    }

    // New top of current weak branch.
    if (!strong)
    {
        log_state_change(*parent, *state);
        cache(block_ptr, state);
        handler(error::success, height);
        return;
    }

    // Reorganize candidate chain.
    // ........................................................................

    const auto top_candidate = state_->height();
    if (branch_point > top_candidate)
    {
        handler(suspend(error::store_integrity), height);
        return;
    }

    if (branch_point < top_candidate)
    {
        // Implies that all blocks above branch are weak (restart downloads).
        notify(error::success, chase::regressed, branch_point);
    }

    // Pop down to the branch point.
    auto index = top_candidate;
    while (index > branch_point)
    {
        if (!query.pop_candidate())
        {
            handler(suspend(query.get_code()), height);
            return;
        }

        fire(events::header_reorganized, index--);
    }

    // branch_point + 1
    ++index;

    // Push stored strong headers to candidate chain.
    for (const auto& link: views_reverse(store_branch))
    {
        if (!query.push_candidate(link))
        {
            handler(suspend(query.get_code()), height);
            return;
        }

        fire(events::header_organized, index++);
    }

    // Store strong tree headers and push to candidate chain.
    for (const auto& key: views_reverse(tree_branch))
    {
        if (!push(key))
        {
            handler(suspend(query.get_code()), height);
            return;
        }

        fire(events::header_archived, index);
        fire(events::header_organized, index++);
    }

    // Push new header as top of candidate chain.
    {
        if (push(block, state->context()).is_terminal())
        {
            handler(suspend(query.get_code()), height);
            return;
        }
        
        fire(events::header_archived, index);
        fire(events::header_organized, index++);
    }

    // Reset top chain state cache and notify.
    // ........................................................................

    // Delay so headers can get current before block download starts.
    // Checking currency before notify also avoids excessive work backlog.
    if (is_block() || is_current(header.timestamp()))
    {
        // If at start the fork point is top of both chains, and next candidate
        // is already downloaded, then new header will arrive and download will
        // be skipped, resulting in stall until restart at which time the start
        // event will advance through all downloaded candidates and progress on
        // arrivals. This bumps preconfirmation for current strong headers.
        notify(error::success, chase::bump, add1(branch_point));

        // Start block downloads, which upon completion bumps preconfirmation.
        notify(error::success, chase_object(), branch_point);
    }

    // Logs from candidate block parent to the candidate (forward sequential).
    log_state_change(*parent, *state);
    state_ = state;

    handler(error::success, height);
}

TEMPLATE
void CLASS::do_disorganize(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace system;

    // Skip already reorganized out, get height.
    // ........................................................................

    // Upon restart candidate chain validation will hit unconfirmable block.
    if (closed())
        return;

    // If header is not a current candidate it has been reorganized out.
    // If header becomes candidate again its unconfirmable state is handled.
    auto& query = archive();
    if (!query.is_candidate_block(link))
        return;

    size_t height{};
    if (!query.get_height(height, link) || is_zero(height))
    {
        suspend(error::store_integrity);
        return;
    }

    // Must reorganize down to fork point, since entire branch is now weak.
    const auto fork_point = query.get_fork();
    if (height <= fork_point)
    {
        suspend(error::store_integrity);
        return;
    }

    // Get fork point chain state.
    // ........................................................................

    auto state = query.get_candidate_chain_state(settings_, fork_point);
    if (!state)
    {
        suspend(error::store_integrity);
        return;
    }

    // Copy candidates from above fork point to top into header tree.
    // ........................................................................
    // Forward order is required to advance chain state for tree.

    const auto top_candidate = state_->height();
    for (auto index = add1(fork_point); index <= top_candidate; ++index)
    {
        typename Block::cptr block{};
        if (!get_block(block, index))
        {
            suspend(error::store_integrity);
            return;
        }

        const auto& header = get_header(*block);
        state.reset(new chain::chain_state{ *state, header, settings_ });
        cache(block, state);
    }

    // Pop candidates from top down to above fork point.
    // ........................................................................
    // Can't pop in loop above because state chaining requires forward order.

    for (auto index = top_candidate; index > fork_point; --index)
    {
        if (!query.pop_candidate())
        {
            suspend(query.get_code());
            return;
        }

        fire(events::header_reorganized, index);
    }

    // Push confirmed headers from above fork point onto candidate chain.
    // ........................................................................

    const auto top_confirmed = query.get_top_confirmed();
    for (auto index = add1(fork_point); index <= top_confirmed; ++index)
    {
        if (!query.push_candidate(query.to_confirmed(index)))
        {
            suspend(query.get_code());
            return;
        }

        fire(events::header_organized, index);
    }

    state = query.get_candidate_chain_state(settings_, top_confirmed);

    // Logs from previous top candidate to previous fork point (jumps back).
    log_state_change(*state_, *state);
    state_ = state;

    // Notify check/download/confirmation to reset to top (clear).
    // As this organizer controls the candidate array, height is definitive.
    notify(error::success, chase::disorganized, top_confirmed);
}

// Private
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::cache(const typename Block::cptr& block_ptr,
    const chain_state::ptr& state) NOEXCEPT
{
    tree_.insert({ block_ptr->hash(), { block_ptr, state } });
}

TEMPLATE
CLASS::chain_state::ptr CLASS::get_chain_state(
    const system::hash_digest& previous_hash) const NOEXCEPT
{
    if (!state_)
        return {};

    // Top state is cached because it is by far the most commonly retrieved.
    if (state_->hash() == previous_hash)
        return state_;

    // Previous block may be cached because it is not yet strong.
    const auto it = tree_.find(previous_hash);
    if (it != tree_.end())
        return it->second.state;

    // previous_hash may or not exist and/or be a candidate.
    return archive().get_chain_state(settings_, previous_hash);
}

// Also obtains branch point for work summation termination.
// Also obtains ordered branch identifiers for subsequent reorg.
TEMPLATE
bool CLASS::get_branch_work(uint256_t& work, size_t& branch_point,
    system::hashes& tree_branch, header_links& store_branch,
    const system::chain::header& header) const NOEXCEPT
{
    // Use pointer to avoid const/copy.
    auto previous = &header.previous_block_hash();
    const auto& query = archive();
    work = header.proof();

    // Sum all branch work from tree.
    for (auto it = tree_.find(*previous); it != tree_.end();
        it = tree_.find(*previous))
    {
        const auto& next = get_header(*it->second.block);
        previous = &next.previous_block_hash();
        tree_branch.push_back(next.hash());
        work += next.proof();
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

    // Height of the highest candidate header is the branch point.
    return query.get_height(branch_point, link);
}

// A branch with greater work will cause candidate reorganization.
TEMPLATE
bool CLASS::get_is_strong(bool& strong, const uint256_t& branch_work,
    size_t branch_point) const NOEXCEPT
{
    uint256_t candidate_work{};
    const auto& query = archive();

    for (auto height = query.get_top_candidate(); height > branch_point;
        --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_candidate(height)))
            return false;

        // Not strong when candidate_work equals or exceeds branch_work.
        candidate_work += system::chain::header::proof(bits);
        if (candidate_work >= branch_work)
        {
            strong = false;
            return true;
        }
    }

    strong = true;
    return true;
}

TEMPLATE
database::header_link CLASS::push(const Block& block,
    const system::chain::context& context) const NOEXCEPT
{
    using namespace system;
    auto& query = archive();
    const auto link = query.set_link(block, database::context
    {
        possible_narrow_cast<database::context::flag::integer>(context.flags),
        possible_narrow_cast<database::context::block::integer>(context.height),
        context.median_time_past,
    });

    return query.push_candidate(link) ? link : database::header_link{};
}

TEMPLATE
bool CLASS::push(const system::hash_digest& key) NOEXCEPT
{
    const auto value = tree_.extract(key);
    BC_ASSERT_MSG(!value.empty(), "missing tree value");

    auto& query = archive();
    const auto& it = value.mapped();
    const auto link = query.set_link(*it.block, it.state->context());
    return query.push_candidate(link);
}

TEMPLATE
void CLASS::log_state_change(const chain_state& from,
    const chain_state& to) const NOEXCEPT
{
    if constexpr (network::levels::news_defined)
    {
        using namespace system;

        if (from.flags() != to.flags())
        {
            const binary prev{ flag_bits, to_big_endian(from.flags()) };
            const binary next{ flag_bits, to_big_endian(to.flags()) };

            LOGN("Fork flags changed from ["
                << prev << "] to ["
                << next << "] at ["
                << to.height() << ":" << encode_hash(to.hash()) << "].");
        }

        if (from.minimum_block_version() != to.minimum_block_version())
        {
            LOGN("Minimum block version changed from ["
                << from.minimum_block_version() << "] to ["
                << to.minimum_block_version()   << "] at ["
                << to.height() << ":" << encode_hash(to.hash()) << "].");
        }
    }
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin

#endif
