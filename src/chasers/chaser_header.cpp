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
#include <bitcoin/node/chasers/chaser_header.hpp>

#include <ranges>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_header

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Public
// ----------------------------------------------------------------------------

// static
code chaser_header::validate(const header& header, const chain_state& state,
    const system::settings& settings) NOEXCEPT
{
    if (const auto ec = header.check(
        settings.timestamp_limit_seconds,
        settings.proof_of_work_limit,
        settings.forks.ltc_scrypt_proof_of_work))
        return ec;

    if (const auto ec = header.accept(state.context(),
        settings.retargeting_interval()))
        return ec;

    const auto height = state.height();
    const auto& checkpoints = settings.checkpoints;
    if (checkpoint::is_conflict(checkpoints, header.get_hash(), height))
        return system::error::checkpoint_conflict;

    return error::success;
}

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser(node),
    settings_(system_settings()),
    checkpoints_(system_settings().checkpoints)
{
}

code chaser_header::start() NOEXCEPT
{
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

    LOGN("Candidate top [" << encode_hash(state_->hash()) << ":"
        << state_->height() << "].");

    update_checkpoint(top);
    prune_tree(top);
    SUBSCRIBE_CHASE(handle_chase, _1, _2);
    return error::success;
}

void chaser_header::organize(const header::cptr& header,
    organize_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_organize, header, false, false, false, std::move(handler));
}

void chaser_header::organize(const header::cptr& header, bool milestone,
    organize_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_organize, header, false, milestone, true, std::move(handler));
}

void chaser_header::prioritize(const hash_digest& hash,
    organize_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_prioritize, hash, std::move(handler));
}

// Methods
// ----------------------------------------------------------------------------

bool chaser_header::handle_chase(const code&, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (to_chase(value))
    {
        // Roll back the candidate chain to confirmed top (via fork point).
        case chase::unchecked:
        {
            if (database_settings().mark_unconfirmable)
            {
                POST(do_disorganize, to_payload<chase::unchecked>(value).link);
            }

            break;
        }
        case chase::unvalid:
        {
            if (database_settings().mark_unconfirmable)
            {
                POST(do_disorganize, to_payload<chase::unvalid>(value).link);
            }

            break;
        }
        case chase::unconfirmable:
        {
            if (database_settings().mark_unconfirmable)
            {
                POST(do_disorganize,
                    to_payload<chase::unconfirmable>(value).link);
            }

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

void chaser_header::do_organize(const header::cptr& header_ptr,
    bool prioritized, bool milestone, bool proven,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto& query = archive();
    const auto& header = *header_ptr;
    const auto& hash = header.get_hash();

    // Skip existing/orphan, get state.
    // ........................................................................

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    const auto it = tree_.find(hash);
    if (it != tree_.cend())
    {
        const auto& state = it->second->get_state();
        handler(error::duplicate_header, state->height());
        return;
    }

    size_t height{};
    if (const auto ec = duplicate(height, hash))
    {
        handler(ec, height);
        return;
    }

    // Validate parent and obtain header chain state.
    // ........................................................................

    // Shortcircuit parent unconfirmable (looping over failed block).
    const auto& previous = header.previous_block_hash();
    if (query.is_unconfirmable(query.to_header(previous)))
    {
        handler(database::error::block_unconfirmable, {});
        return;
    }

    // Shortcircuit fork at/under the top reached checkpoint.
    if (is_under_active_checkpoint(previous))
    {
        handler(system::error::checkpoint_conflict, {});
        return;
    }

    // Obtain parent state from state_, tree, or store as applicable.
    const auto parent = get_chain_state(previous);
    if (!parent)
    {
        handler(error::orphan_header, {});
        return;
    }

    // Roll chain state forward from archived parent to new header.
    const auto state = emplace_shared<chain_state>(*parent, header, settings_);
    height = state->height();

    if (checkpoint::is_conflict(checkpoints_, hash, height))
    {
        handler(system::error::checkpoint_conflict, height);
        return;
    }

    // A proven header was validated by the protocol, all are storable.
    if (!proven)
    {
        if (const auto ec = validate(header, *state, settings_))
        {
            handler(ec, height);
            return;
        }
    }

    // Compute relative work.
    // ........................................................................

    uint256_t work{};
    hashes tree_branch{};
    header_states store_branch{};
    if (!get_branch_work(work, tree_branch, store_branch, header))
    {
        handler(fault(error::organize2), height);
        return;
    }

    bool strong{};
    const auto branch_size = tree_branch.size() + store_branch.size();
    const auto branch_point = height - add1(branch_size);
    if (!query.get_strong_branch(strong, work, branch_point, prioritized))
    {
        handler(fault(error::organize3), height);
        return;
    }

    // New top of a weak branch.
    if (!strong)
    {
        log_state_change(*parent, *state);
        cache(header_ptr, state);
        handler(error::success, height);
        return;
    }

    // Reorganize candidate chain.
    // ........................................................................

    // Cannot be branching above top.
    auto top = state_->height();
    if (branch_point > top)
    {
        handler(fault(error::organize4), height);
        return;
    }

    // Pop top down to the branch point.
    const auto regress = branch_point < top;
    while (branch_point < top)
    {
        if (!set_reorganized(top--))
        {
            handler(fault(error::organize5), height);
            return;
        }
    }

    // Reset chasers to the branch point.
    if (regress)
    {
        notify(error::success, chases::regressed{ branch_point });
    }

    // Push stored strong headers to candidate chain.
    for (const auto& stored: std::views::reverse(store_branch))
    {
        if (!set_organized(stored.link, ++top))
        {
            handler(fault(error::organize6), height);
            return;
        }
    }

    // Archive strong tree headers and push to candidate chain.
    for (const auto& key: std::views::reverse(tree_branch))
    {
        if (const auto ec = push_header(key))
        {
            handler(fault(ec), height);
            return;
        }

        top++;
    }

    // Push new header as top of candidate chain.
    if (const auto ec = push_header(header, state->context(), milestone))
    {
        handler(fault(ec), height);
        return;
    }

    // Reset top chain state and notify.
    // ........................................................................

    // Delay so headers can get current before block download starts.
    // Checking currency before notify also avoids excessive work backlog.
    const auto current = is_current_time(header.timestamp());
    if (current)
    {
        if (!bumped_ || regress)
        {
            // If at start the fork point is top of both chains, and next
            // candidate is already downloaded, then new header will arrive and
            // download will be skipped, resulting in stall until restart at
            // which time the start event will advance through all downloaded
            // candidates and progress on arrivals. This bumps validation once
            // for current strong headers, and again on regression, as the
            // candidate above the branch point may already be downloaded when
            // reorganizing back to a stored branch.
            notify(error::success, chases::bump{ add1(branch_point) });
            bumped_ = true;
        }

        // This prevents download stall, the check chaser races ahead.
        // Start block downloads, which upon completion bumps validation.
        notify(error::success, chases::headers{ branch_point });
    }

    // Logs from candidate block parent to the candidate (forward sequential).
    log_state_change(*parent, *state);
    state_ = state;

    // Advance top reached checkpoint and prune the tree.
    update_checkpoint(height);
    prune_tree(height);
    shrink_tree(current);
    handler(error::success, height);
}

// bitcoind's preciousblock, a manual tie break between equal work branches.
// The preference is not retained, as the reorganized branch then wins ties.
void chaser_header::do_prioritize(const hash_digest& hash,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    // Only the top of a cached branch can tie the candidate top.
    if (std::any_of(tree_.cbegin(), tree_.cend(), [&](const auto& item) NOEXCEPT
        {
            return item.second->previous_block_hash() == hash;
        }))
    {
        handler(error::success, {});
        return;
    }

    // A tied branch is cached, extract it for reevaluation as prioritized.
    auto handle = tree_.extract(hash);
    if (!handle)
    {
        handler(database::error::not_found, {});
        return;
    }

    do_organize(handle.mapped(), true, false, true, handler);
}

void chaser_header::do_disorganize(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
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
        fault(error::organize7);
        return;
    }

    for (const auto& candidate: candidates)
    {
        const auto header_ptr = query.get_header(candidate);
        if (is_null(header_ptr))
        {
            fault(error::organize8);
            return;
        }

        state = to_shared<chain_state>(*state, *header_ptr, settings_);
        cache(header_ptr, state);
    }

    // Pop invalids (top to link), set unconfirmable (stops validation).
    // ........................................................................

    for (const auto& invalid: std::views::reverse(invalids))
    {
        if (!query.set_block_unconfirmable(invalid))
        {
            fault(error::organize9);
            return;
        }

        if (!set_reorganized(invalid))
        {
            fault(error::organize10);
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
    for (const auto& confirmed: query.get_confirmed_fork(fork))
    {
        if (!set_organized(confirmed, ++fork_point))
        {
            fault(error::organize12);
            return;
        }
    }

    // Reset top candidate state to match confirmed, log and notify.
    // ........................................................................

    // fork_point reflects the new candidate top.
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
    notify(error::success, chases::disorganized{ fork_point });

    // Reset all connections to ensure that new connections exist.
    notify(error::success, chases::suspend{});
}

// Validation (private).
// ----------------------------------------------------------------------------

code chaser_header::duplicate(size_t& height,
    const hash_digest& hash) const NOEXCEPT
{
    // With a candidate reorg that drops strong below a valid header chain,
    // this will cause a sequence of headers to be bypassed, such that a
    // parent of a block that doesn't exist will not be a candidate, which
    // result in a failure of get_chain_state, because it depends on candidate
    // state. So get_chain_state needs to be chain independent.

    height = max_size_t;
    const auto& query = archive();
    const auto id = query.to_header(hash);
    if (!id.is_terminal())
    {
        // database::error::block_unconfirmable
        // database::error::block_confirmable
        // database::error::block_valid
        // database::error::unknown_state (debugging)
        // database::error::unvalidated
        const auto ec = query.get_header_state(id);

        // All header states are duplicates, one implies fail.
        if (ec == database::error::block_unconfirmable)
        {
            height = query.get_height(id);
            return ec;
        }

        // height set to max_size_t unless unconfirmable.
        return error::duplicate_header;
    }

    return error::success;
}

// Setters (private).
// ----------------------------------------------------------------------------

bool chaser_header::set_reorganized(height_t candidate_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT(!is_under_checkpoint(candidate_height));
    if (!archive().pop_candidate())
        return false;

    fire(events::header_reorganized, candidate_height);
    LOGV("Header reorganized: " << candidate_height);
    return true;
}

bool chaser_header::set_organized(const header_link& link,
    height_t candidate_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

#if !defined(NDEBUG)
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
#endif // !NDEBUG

    if (!query.push_candidate(link))
        return false;

    fire(events::header_organized, candidate_height);
    LOGV("Header organized: " << candidate_height);
    return true;
}

// Headers cannot be set strong, that is only when the block is archived.
// Milestone is archived in the header and like checkpoint cannot change.
// But unlike checkpointed, milestoned blocks may not be strong chain.
code chaser_header::push_header(const header& header, const context& ctx,
    bool milestone) NOEXCEPT
{
    auto& query = archive();
    header_link link{};
    const auto ec = query.set_code(link, header, ctx, milestone, false);
    if (ec)
        return ec;

    fire(events::header_archived, ctx.height);
    LOGV("Header archived: " << ctx.height);
    return set_organized(link, ctx.height) ? error::success : error::organize14;
}

code chaser_header::push_header(const hash_digest& key) NOEXCEPT
{
    const auto handle = tree_.extract(key);
    if (!handle)
        return error::organize15;

    const auto& header_ptr = handle.mapped();
    const auto& state = header_ptr->get_state();
    return push_header(*header_ptr, state->context(), false);
}

void chaser_header::cache(const header::cptr& header,
    const chain_state::cptr& state) NOEXCEPT
{
    // Any header obtained from the tree must have state cached.
    header->set_state(state);

    tree_.emplace(header->get_hash(), header);
}

// Checkpoint gate (private).
// ----------------------------------------------------------------------------

bool chaser_header::is_under_active_checkpoint(
    const hash_digest& previous) const NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (is_zero(active_checkpoint_))
        return false;

    // Extending the candidate top (the common case, necessarily above).
    if (state_->hash() == previous)
        return false;

    // Tree headers are necessarily above (purged as checkpoints are reached).
    if (tree_.find(previous) != tree_.end())
        return false;

    // Unstored parent is the orphan case (handled downstream).
    const auto link = query.to_header(previous);
    if (link.is_terminal())
        return false;

    // The new header is a child, so at/under when its parent is under.
    return query.get_height(link) < active_checkpoint_;
}

// Set the highest checkpoint reached in the candidate chain.
void chaser_header::update_checkpoint(height_t top) NOEXCEPT
{
    if (top < next_checkpoint_)
        return;

    next_checkpoint_ = max_size_t;
    const auto previous = active_checkpoint_;
    for (const auto& item: checkpoints_)
    {
        if (item.height() <= top)
            active_checkpoint_ = std::max(active_checkpoint_, item.height());
        else
            next_checkpoint_ = std::min(next_checkpoint_, item.height());
    }

    if (active_checkpoint_ != previous)
    {
        LOGV("Checkpoint [" << active_checkpoint_ << "] reached.");
        prune_tree();
    }
}

// Tree control (private).
// ----------------------------------------------------------------------------

void chaser_header::shrink_tree(bool current) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (shrunk_ || !current)
        return;

    shrunk_ = true;
    tree_ = { tree_.cbegin(), tree_.cend() };
    LOGV("Tree buckets reduced to (" << tree_.bucket_count() << ").");
}

// Purged branches have top work under the window (dead branches).
void chaser_header::prune_tree(const uint256_t& threshold) NOEXCEPT
{
    std::unordered_map<hash_digest, size_t> children{};
    for (const auto& item: tree_)
        ++children[item.second->previous_block_hash()];

    hashes tops{};
    for (const auto& item: tree_)
        if (children.find(item.first) == children.end())
            tops.push_back(item.first);

    size_t count{};
    for (const auto& top: tops)
    {
        auto it = tree_.find(top);
        if (it == tree_.end())
            continue;

        const auto& state = it->second->get_state();
        if (state->cumulative_work() >= threshold)
            continue;

        // Descend while each parent has no other child (copy before erase).
        while (true)
        {
            const auto& head = *it->second;
            const hash_digest previous{ head.previous_block_hash() };
            tree_.erase(it);
            ++count;

            const auto child = children.find(previous);
            if (child == children.end() || !is_zero(--child->second))
                break;

            it = tree_.find(previous);
            if (it == tree_.end())
                break;
        }
    }

    if (!is_zero(count))
    {
        LOGN("Purged (" << count << ") headers under window at ["
            << state_->height() << "].");
    }
}

// Purge once per window of candidate progress, never on arrival.
void chaser_header::prune_tree(height_t top) NOEXCEPT
{
    if (top < next_window_)
        return;

    const auto minutes = node_settings().currency_window_minutes;
    const auto spacing = settings_.block_spacing_seconds;
    const auto window = (minutes * 60u) / spacing;
    if (is_zero(window))
        return;

    // Threshold is the candidate top work less a window of its proof.
    next_window_ = top + window;
    const auto& work = state_->cumulative_work();
    const auto proof = chain::header::proof(state_->work_required());
    const auto span = uint256_t{ window } * proof;
    if (work > span)
        prune_tree(work - span);
}

// Purged headers conflict with the reached checkpoint (dead branches).
void chaser_header::prune_tree() NOEXCEPT
{
    const auto count = std::erase_if(tree_, [this](const auto& item) NOEXCEPT
    {
        const auto& state = item.second->get_state();
        return state->height() <= active_checkpoint_;
    });

    if (!is_zero(count))
    {
        LOGN("Purged (" << count << ") headers under checkpoint ["
            << active_checkpoint_ << "].");
    }
}

// Getters (private).
// ----------------------------------------------------------------------------

chaser_header::chain_state::cptr chaser_header::get_chain_state(
    const hash_digest& previous_hash) const NOEXCEPT
{
    if (!state_)
        return {};

    // Top state is cached because it is by far the most commonly retrieved.
    if (state_->hash() == previous_hash)
        return state_;

    // Previous header may be cached because it is not yet strong.
    const auto it = tree_.find(previous_hash);
    if (it != tree_.end())
        return it->second->get_state();

    // previous_hash may or not exist and/or be a candidate.
    return archive().get_confirmed_chain_state(settings_, previous_hash);
}

// Also obtains branch point for work summation termination.
// Also obtains ordered branch identifiers for subsequent reorg.
bool chaser_header::get_branch_work(uint256_t& work, hashes& tree_branch,
    header_states& store_branch, const header& header) const NOEXCEPT
{
    const auto& query = archive();
    hash_cref previous{ header.previous_block_hash() };
    work = header.proof();

    // Get portion of branch from tree and sum its work.
    auto it = tree_.find(previous);
    while (it != tree_.end())
    {
        // Accumulate.
        const auto& head = *it->second;
        tree_branch.push_back(head.hash());
        work += head.proof();

        // Iterate.
        previous = { head.previous_block_hash() };
        it = tree_.find(previous);
    }

    // Get portion of branch that is already stored.
    if (!query.get_branch(store_branch, previous))
        return false;

    // If store_branch is empty then previous is candidate/branch_point.
    if (!store_branch.empty())
    {
        uint256_t store_work{};
        if (!query.get_work(store_work, store_branch))
            return false;

        work += store_work;
    }

    return true;
}

// Logging
// ----------------------------------------------------------------------------

void chaser_header::log_state_change(const chain_state& from,
    const chain_state& to) const NOEXCEPT
{
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

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
