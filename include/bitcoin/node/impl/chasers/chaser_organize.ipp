/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

#include <algorithm>
#include <ranges>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

// Public
// ----------------------------------------------------------------------------

TEMPLATE
CLASS::chaser_organize(full_node& node) NOEXCEPT
  : chaser(node),
    settings_(config().bitcoin),
    checkpoints_(config().bitcoin.checkpoints)
{
}

TEMPLATE
code CLASS::start() NOEXCEPT
{
    using namespace std::placeholders;

    // Initialize cache of top candidate chain state.
    // Spans full chain to obtain cumulative work. This can be optimized by
    // storing it with each header, though the scan is fast. The same occurs
    // when a block first branches below the current chain top. Chain work
    // is a questionable DoS protection scheme only, so could also toss it.
    const auto& query = archive();
    const auto top = query.get_top_candidate();
    state_ = query.get_candidate_chain_state(settings_, top);

    if (!state_)
    {
        fault(error::organize1);
        return error::organize1;
    }

    LOGN("Candidate top [" << system::encode_hash(state_->hash()) << ":"
        << state_->height() << "].");

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

TEMPLATE
void CLASS::organize(const typename Block::cptr& block,
    organize_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_organize, block, std::move(handler));
}

// Methods
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::handle_event(const code&, chase event_, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::unchecked:
        case chase::unvalid:
        case chase::unconfirmable:
        {
            // Roll back the candidate chain to confirmed top (via fork point).
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_disorganize, std::get<header_t>(value));
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
void CLASS::do_organize(typename Block::cptr block,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace system;
    const auto& hash = block->get_hash();
    const auto& header = get_header(*block);

    // Skip existing/orphan, get state.
    // ........................................................................
 
    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    const auto it = tree_.find(hash_cref(hash));
    if (it != tree_.end())
    {
        handler(error_duplicate(), it->second.state->height());
        return;
    }

    size_t height{};
    if (const auto ec = duplicate(height, hash))
    {
        handler(ec, height);
        return;
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
    height = state->height();

    // Validation and currency.
    // ........................................................................

    if (chain::checkpoint::is_conflict(checkpoints_, hash, height))
    {
        handler(system::error::checkpoint_conflict, height);
        return;
    };

    // Headers are late validated, with malleations ignored upon download.
    // Blocks are fully validated (not confirmed), so malleation is non-issue.
    if (const auto ec = validate(*block, *state))
    {
        handler(ec, height);
        return;
    }

    // Cache headers until the branch is sufficiently guaranteed.
    if (!is_storable(*state))
    {
        log_state_change(*parent, *state);
        cache(block, state);
        handler(error::success, height);
        return;
    }

    // Compute relative work.
    // ........................................................................

    bool strong{};
    uint256_t work{};
    hashes tree_branch{};
    height_t branch_point{};
    header_links store_branch{};

    if (!get_branch_work(work, branch_point, tree_branch, store_branch, header))
    {
        handler(fault(error::organize2), height);
        return;
    }

    // branch_point is the highest tree-candidate common block.
    if (!get_is_strong(strong, work, branch_point))
    {
        handler(fault(error::organize3), height);
        return;
    }

    // New top of a weak branch.
    if (!strong)
    {
        log_state_change(*parent, *state);
        cache(block, state);
        handler(error::success, height);
        return;
    }

    // Reorganize candidate chain.
    // ........................................................................

    // The milestone flag will be archived in the header record.
    // Here it must be computed from the header tree because it trickles down.
    update_milestone(header, height, branch_point);

    const auto top_candidate = state_->height();
    if (branch_point > top_candidate)
    {
        handler(fault(error::organize4), height);
        return;
    }

    // Pop down to the branch point.
    auto index = top_candidate;
    while (index > branch_point)
    {
        if (!set_reorganized(index--))
        {
            handler(fault(error::organize5), height);
            return;
        }
    }

    // Reset chasers to the branch point.
    if (branch_point < top_candidate)
    {
        notify(error::success, chase::regressed, branch_point);
    }

    // Push stored strong headers to candidate chain.
    for (const auto& link: std::views::reverse(store_branch))
    {
        if (!set_organized(link, index++))
        {
            handler(fault(error::organize6), height);
            return;
        }
    }

    // Store strong tree headers and push to candidate chain.
    for (const auto& key: std::views::reverse(tree_branch))
    {
        if (const auto ec = push_block(key))
        {
            handler(fault(ec), height);
            return;
        }

        index++;
    }

    // Push new header as top of candidate chain.
    if (const auto ec = push_block(*block, state->context()))
    {
        handler(fault(ec), height);
        return;
    }

    // Reset top chain state and notify.
    // ........................................................................

    // Delay so headers can get current before block download starts.
    // Checking currency before notify also avoids excessive work backlog.
    if (is_block() || is_current(header.timestamp()))
    {
        if (!bumped_)
        {
            // If at start the fork point is top of both chains, and next candidate
            // is already downloaded, then new header will arrive and download will
            // be skipped, resulting in stall until restart at which time the start
            // event will advance through all downloaded candidates and progress on
            // arrivals. This bumps validation once for current strong headers.
            notify(error::success, chase::bump, add1(branch_point));
            bumped_ = true;
        }

        // chase::headers | chase::blocks
        // This prevents download stall, the check chaser races ahead.
        // Start block downloads, which upon completion bumps validation.
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
    auto& query = archive();

    if (closed())
        return;

    // May have been reorganized already by previous unconfirmable.
    if (!query.is_candidate_header(link))
        return;

    // Get list of links to pop (weak branch), may be empty (previous disorg).
    // ........................................................................

    // Guarded by confirmed interlock, ensures a consistent branch only.
    size_t fork_point{};
    auto candidates = query.get_candidate_fork(fork_point);

    // Move candidates above the invalid link into an independent list.
    header_links invalids{};
    if (!part(candidates, invalids, link))
        return;

    // Copy valid portion of branch (below link) into header tree with state.
    // ........................................................................

    auto state = query.get_candidate_chain_state(settings_, fork_point);
    if (!state)
    {
        fault(error::organize8);
        return;
    }

    for (const auto& candidate: candidates)
    {
        typename Block::cptr block{};
        if (!get_block(block, candidate))
        {
            fault(error::organize9);
            return;
        }

        const auto& header = get_header(*block);
        state = to_shared<chain::chain_state>(*state, header, settings_);
        cache(block, state);
    }

    // Pop invalids (top to link), set unconfirmable (stops validation).
    // ........................................................................

    for (const auto& invalid: std::views::reverse(invalids))
    {
        if (!query.set_block_unconfirmable(invalid))
        {
            fault(error::organize10);
            return;
        }

        if (!set_reorganized(invalid))
        {
            fault(error::organize11);
            return;
        }
    }

    // Pop weak candidates (below link to fork point).
    // ........................................................................

    for (const auto& candidate: std::views::reverse(candidates))
    {
        if (!set_reorganized(candidate))
        {
            fault(error::organize11);
            return;
        }
    }

    // Push all confirmeds above fork point onto candidate chain.
    // ........................................................................

    // Candidate fork link used to ensure consistency with confirmed chain.
    const auto fork = query.to_candidate(fork_point);

    // Guarded by confirmed interlock, ensures fork point consistency.
    const auto confirmeds = query.get_confirmed_fork(fork);

    // fork_point reflects the new candidate top once complete.
    // May be empty because of reorganization or just no blocks.
    for (const auto& confirmed: std::views::reverse(confirmeds))
    {
        if (!set_organized(confirmed, ++fork_point))
        {
            fault(error::organize12);
            return;
        }
    }

    // Reset top candidate state to match confirmed, log and notify.
    // ........................................................................

    state = query.get_candidate_chain_state(settings_, fork_point);
    if (!state)
    {
        fault(error::organize13);
        return;
    }

    // Logs from previous top candidate to previous fork point (jumps back).
    log_state_change(*state_, *state);
    state_ = state;

    // Candidate is same as confirmed, reset chasers to new top.
    notify(error::success, chase::disorganized, fork_point);

    // Reset all connections to ensure that new connections exist.
    notify(error::success, chase::suspend, {});
}

// Private setters
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::set_reorganized(height_t candidate_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT(!is_under_checkpoint(candidate_height));
    if (!archive().pop_candidate())
        return false;

    fire(events::header_reorganized, candidate_height);
    LOGV("Header reorganized: " << candidate_height);
    return true;
}

TEMPLATE
bool CLASS::set_organized(const database::header_link& link,
    height_t candidate_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    const auto previous_height = query.get_top_candidate();
    if (candidate_height != add1(previous_height))
    {
        fault(error::stalled_channel);
        return false;
    }

    const auto parent = query.to_parent(link);
    const auto top = query.to_candidate(previous_height);
    if (parent != top)
    {
        fault(error::suspended_channel);
        return false;
    }

    if (!query.push_candidate(link))
        return false;

    fire(events::header_organized, candidate_height);
    LOGV("Header organized: " << candidate_height);
    return true;
}

TEMPLATE
code CLASS::push_block(const Block& block,
    const system::chain::context& ctx) NOEXCEPT
{
    // set_code invokes set_strong when checked, but only for a whole block.
    // Headers cannot be set strong, that is only when the block is archived.
    // Milestone is archived in the header and like checkpoint cannot change.
    // But unlike checkpointed, milestoned blocks may not be strong chain.
    const auto checked = is_block() && is_under_checkpoint(ctx.height);
    const auto milestone = is_under_milestone(ctx.height);

    auto& query = archive();
    database::header_link link{};
    const auto ec = query.set_code(link, block, ctx, milestone, checked);
    if (ec)
        return ec;

    if (!set_organized(link, ctx.height))
        return error::organize14;

    // events:header_archived | events:block_archived
    fire(events_object(), ctx.height);
    LOGV("Header archived: " << ctx.height);
    return ec;
}

TEMPLATE
code CLASS::push_block(const system::hash_digest& key) NOEXCEPT
{
    const auto handle = tree_.extract(system::hash_cref(key));
    if (!handle)
        return error::organize15;

    const auto& value = handle.mapped();
    return push_block(*value.block, value.state->context());
}

TEMPLATE
void CLASS::cache(const typename Block::cptr& block,
    const chain_state::cptr& state) NOEXCEPT
{
    // TODO: guard cache against memory exhaustion (DoS).
    tree_.emplace(system::hash_cref(block->get_hash()),
        block_state{ block, state });
}

// Private getters
// ----------------------------------------------------------------------------

TEMPLATE
CLASS::chain_state::cptr CLASS::get_chain_state(
    const system::hash_digest& previous_hash) const NOEXCEPT
{
    if (!state_)
        return {};

    // Top state is cached because it is by far the most commonly retrieved.
    if (state_->hash() == previous_hash)
        return state_;

    // Previous block may be cached because it is not yet strong.
    const auto it = tree_.find(system::hash_cref(previous_hash));
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
    for (auto it = tree_.find(system::hash_cref(*previous)); it != tree_.end();
        it = tree_.find(system::hash_cref(*previous)))
    {
        const auto& next = get_header(*it->second.block);
        previous = &next.previous_block_hash();
        tree_branch.push_back(next.hash());
        work += next.proof();
    }

    // Sum branch work from store.
    database::height_link link{};
    for (link = query.to_header(*previous); !query.is_candidate_header(link);
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

// Logging
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::log_state_change(const chain_state& from,
    const chain_state& to) const NOEXCEPT
{
    using namespace system;
    if constexpr (network::levels::news_defined)
    {
        if (from.flags() != to.flags())
        {
            constexpr auto flag_bits = to_bits(sizeof(chain::flags));
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

} // namespace node
} // namespace libbitcoin

#endif
