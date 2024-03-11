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
#include <bitcoin/node/chasers/chaser_header.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_header
    
using namespace network;
using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser(node),
    minimum_work_(config().bitcoin.minimum_work),
    milestone_(config().bitcoin.milestone),
    checkpoints_(config().bitcoin.checkpoints),
    currency_window_(config().node.currency_window()),
    use_currency_window_(to_bool(config().node.currency_window_minutes))
{
}

chaser_header::~chaser_header() NOEXCEPT
{
}

// start
// ----------------------------------------------------------------------------

code chaser_header::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());

    // Initialize cache of top candidate chain state (expensive).
    // Spans full chain to obtain cumulative work. This can be optimized by
    // storing it with each header, though the scan is fast. The same occurs
    // when a block first branches below the current chain top. Chain work is
    // a questionable DoS protection scheme only, so could also toss it.
    top_state_ = archive().get_candidate_chain_state(
        config().bitcoin, archive().get_top_candidate());

    LOGN("Candidate top ["<< encode_hash(top_state_->hash()) << ":"
        << top_state_->height() << "].");

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// disorganize
// ----------------------------------------------------------------------------

void chaser_header::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    // Posted due to block/header invalidation.
    if (event_ == chase::unchecked || 
        event_ == chase::unconnected ||
        event_ == chase::unconfirmed)
    {
        POST(do_disorganize, std::get<header_t>(value));
    }
}

void chaser_header::do_disorganize(header_t header) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Skip already reorganized out, get height.
    // ------------------------------------------------------------------------

    // Upon restart candidate chain validation will hit unconfirmable block.
    if (closed())
        return;

    // If header is not a current candidate it has been reorganized out.
    // If header becomes candidate again its unconfirmable state is handled.
    auto& query = archive();
    if (!query.is_candidate_block(header))
        return;

    size_t height{};
    if (!query.get_height(height, header) || is_zero(height))
    {
        close(error::store_integrity);
        return;
    }

    // Mark candidates above and pop at/above height.
    // ------------------------------------------------------------------------

    // Pop from top down to and including header marking each as unconfirmable.
    // Unconfirmability isn't necessary for validation but adds query context.
    for (auto index = query.get_top_candidate(); index > height; --index)
    {
        if (!query.set_block_unconfirmable(query.to_candidate(index)) ||
            !query.pop_candidate())
        {
            close(error::store_integrity);
            return;
        }
    }

    // Candidate at height is already marked as unconfirmable by notifier.
    if (!query.pop_candidate())
    {
        close(error::store_integrity);
        return;
    }

    // Copy candidates from above fork point to top into header tree.
    // ------------------------------------------------------------------------

    // There may not be (valid) blocks between fork point and invalid block,
    // but top_state_ must be recomputed and if there are blocks then header
    // tree also requires chain state, so initialize at fork point (expensive).
    const auto& coin = config().bitcoin;
    const auto fork_point = query.get_fork();
    auto state = query.get_candidate_chain_state(coin, fork_point);
    if (!state)
    {
        close(error::store_integrity);
        return;
    }

    const auto top = top_state_->height();
    for (auto index = add1(fork_point); index <= top; ++index)
    {
        const auto save = query.get_header(query.to_candidate(index));
        if (!save)
        {
            close(error::store_integrity);
            return;
        }

        state.reset(new chain_state{ *state, *save, coin });
        cache(save, state);
    }

    // Pop candidates from top to above fork point.
    // ------------------------------------------------------------------------
    for (auto index = top; index > fork_point; --index)
    {
        if (!query.pop_candidate())
        {
            close(error::store_integrity);
            return;
        }
    }

    // Push confirmed headers from above fork point onto candidate chain.
    // ------------------------------------------------------------------------
    const auto strong = query.get_top_confirmed();
    for (auto index = add1(fork_point); index <= strong; ++index)
    {
        if (!query.push_candidate(query.to_confirmed(index)))
        {
            close(error::store_integrity);
            return;
        }
    }

    // Reset top chain state cache.
    // ------------------------------------------------------------------------

    // TODO: No new headers but might need to notify of disorganization.
    top_state_ = state;
}

// organize
// ----------------------------------------------------------------------------

void chaser_header::organize(const header::cptr& header,
    organize_handler&& handler) NOEXCEPT
{
    POST(do_organize, header, std::move(handler));
}

void chaser_header::do_organize(const header::cptr& header_ptr,
    const organize_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    const auto& header = *header_ptr;
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
        handler(error::duplicate_header, {});
        return;
    }

    // If header exists test for prior invalidity as a block.
    const auto link = query.to_header(hash);
    if (!link.is_terminal())
    {
        const auto ec = query.get_header_state(link);
        if (ec == database::error::block_unconfirmable)
        {
            handler(ec, {});
            return;
        }

        handler(error::duplicate_header, {});
        return;
    }

    // Obtains from top_state_, tree, or store as applicable.
    auto state = get_chain_state(header.previous_block_hash());
    if (!state)
    {
        handler(error::orphan_header, {});
        return;
    }

    // Roll chain state forward from previous to current header.
    state.reset(new chain_state{ *state, header, coin });
    const auto height = state->height();

    // Check/Accept header.
    // ------------------------------------------------------------------------
    // Header validations are not bypassed when under checkpoint/milestone.
    // Checkpoints are considered chain not block/header validation.

    code error{ system::error::checkpoint_conflict };
    if (checkpoint::is_conflict(coin.checkpoints, hash, height) ||
        ((error = header.check(coin.timestamp_limit_seconds,
            coin.proof_of_work_limit,coin.scrypt_proof_of_work))) ||
        ((error = header.accept(state->context()))))
    {
        // There is no storage or notification of an invalid header.
        handler(error, height);
        return;
    }

    // ------------------------------------------------------------------------

    // A checkpointed or milestoned branch always gets disk stored. Otherwise
    // branch must be both current and of sufficient chain work to be stored.
    if (!checkpoint::is_at(checkpoints_, height) &&
        !milestone_.equals(hash, height) &&
        !(is_current(header) && state->cumulative_work() >= minimum_work_))
    {
        cache(header_ptr, state);
        handler(error::success, height);
        return;
    }

    // Compute relative work.
    // ------------------------------------------------------------------------

    size_t point{};
    uint256_t work{};
    hashes tree_branch{};
    header_links store_branch{};
    if (!get_branch_work(work, point, tree_branch, store_branch, header))
    {
        handler(error::store_integrity, height);
        close(error::store_integrity);
        return;
    }

    bool strong{};
    if (!get_is_strong(strong, work, point))
    {
        handler(error::store_integrity, height);
        close(error::store_integrity);
        return;
    }

    if (!strong)
    {
        // Header is new top of current weak branch.
        cache(header_ptr, state);
        handler(error::success, height);
        return;
    }

    // Reorganize candidate chain.
    // ------------------------------------------------------------------------

    auto top = top_state_->height();
    if (top < point)
    {
        handler(error::store_integrity, height);
        close(error::store_integrity);
        return;
    }

    // Pop down to the branch point.
    while (top-- > point)
    {
        if (!query.pop_candidate())
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }
    }

    // Push stored strong headers to candidate chain.
    for (const auto& id: views_reverse(store_branch))
    {
        if (!query.push_candidate(id))
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }
    }

    // Store strong tree headers and push to candidate chain.
    for (const auto& key: views_reverse(tree_branch))
    {
        if (!push_header(key))
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }
    }

    // Push new header as top of candidate chain.
    if (push_header(header_ptr, state->context()).is_terminal())
    {
        handler(error::store_integrity, height);
        close(error::store_integrity);
        return;
    }

    // Reset top chain state cache.
    // ------------------------------------------------------------------------

    top_state_ = state;
    const auto branch_point = possible_narrow_cast<height_t>(point);
    notify(error::success, chase::header, branch_point );
    handler(error::success, height);
}

// utilities
// ----------------------------------------------------------------------------

chain_state::ptr chaser_header::get_chain_state(
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

    // Branch forms from a candidate block below top candidate (expensive).
    size_t height{};
    const auto& query = archive();
    if (query.get_height(height, query.to_header(hash)))
        return query.get_candidate_chain_state(config().bitcoin, height);

    return {};
}

bool chaser_header::is_current(const header& header) const NOEXCEPT
{
    if (!use_currency_window_)
        return true;

    // en.wikipedia.org/wiki/Time_formatting_and_storage_bugs#Year_2106
    const auto time = wall_clock::from_time_t(header.timestamp());
    const auto current = wall_clock::now() - currency_window_;
    return time >= current;
}

bool chaser_header::get_branch_work(uint256_t& work, size_t& point,
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
        previous = &it->second.header->previous_block_hash();
        tree_branch.push_back(it->second.header->hash());
        work += it->second.header->proof();
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
bool chaser_header::get_is_strong(bool& strong, const uint256_t& work,
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

void chaser_header::cache(const header::cptr& header,
    const chain_state::ptr& state) NOEXCEPT
{
    tree_.insert({ header->hash(), { header, state } });
}

database::header_link chaser_header::push_header(const header::cptr& header,
    const context& context) const NOEXCEPT
{
    auto& query = archive();
    const auto link = query.set_link(*header, database::context
    {
        possible_narrow_cast<flags_t>(context.forks),
        possible_narrow_cast<height_t>(context.height),
        context.median_time_past,
    });

    return query.push_candidate(link) ? link : database::header_link{};
}

bool chaser_header::push_header(const hash_digest& key) NOEXCEPT
{
    const auto value = tree_.extract(key);
    BC_ASSERT_MSG(!value.empty(), "missing tree value");

    auto& query = archive();
    const auto& node = value.mapped();
    const auto link = query.set_link(*node.header, node.state->context());
    return query.push_candidate(link);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
