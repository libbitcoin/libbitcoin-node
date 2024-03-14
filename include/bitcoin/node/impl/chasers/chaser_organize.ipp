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

#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define TEMPLATE template <typename Block>
#define CLASS chaser_organize<Block>

BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// public
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
    BC_ASSERT(node_stranded());

    using namespace system;
    using namespace std::placeholders;

    // Initialize cache of top candidate chain state.
    // Spans full chain to obtain cumulative work. This can be optimized by
    // storing it with each header, though the scan is fast. The same occurs
    // when a block first branches below the current chain top. Chain work
    // is a questionable DoS protection scheme only, so could also toss it.
    state_ = archive().get_candidate_chain_state(settings_,
        archive().get_top_candidate());

    // Avoid double logging.
    if constexpr (is_same_type<Block, chain::header>)
    {
        LOGN("Candidate top [" << encode_hash(state_->hash()) << ":"
            << state_->height() << "].");
    }

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

TEMPLATE
void CLASS::organize(const typename Block::cptr& block_ptr,
    organize_handler&& handler) NOEXCEPT
{
    POST(do_organize, block_ptr, std::move(handler));
}

// protected
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

TEMPLATE
void CLASS::handle_event(const code&, chase event_, link value) NOEXCEPT
{
    // Block chaser doesn't need to capture unchecked/unconnected (but okay).
    if (event_ == chase::unchecked ||
        event_ == chase::unconnected ||
        event_ == chase::unconfirmed)
    {
        POST(do_disorganize, std::get<header_t>(value));
    }
}

TEMPLATE
void CLASS::do_disorganize(header_t header) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace system;

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
        close(error::internal_error);
        return;
    }

    const auto fork_point = query.get_fork();
    if (height <= fork_point)
    {
        close(error::internal_error);
        return;
    }

    // Mark candidates above and pop at/above height.
    // ------------------------------------------------------------------------

    // Pop from top down to and including header marking each as unconfirmable.
    // Unconfirmability isn't necessary for validation but adds query context.
    for (auto index = query.get_top_candidate(); index > height; --index)
    {
        const auto link = query.to_candidate(index);

        LOGN("Invalidating candidate ["
            << encode_hash(query.get_header_key(link))<< ":" << index << "].");

        if (!query.set_block_unconfirmable(link) || !query.pop_candidate())
        {
            close(error::store_integrity);
            return;
        }
    }

    LOGN("Invalidating candidate ["
        << encode_hash(query.get_header_key(header)) << ":" << height << "].");

    // Candidate at height is already marked as unconfirmable by notifier.
    if (!query.pop_candidate())
    {
        close(error::store_integrity);
        return;
    }

    // Reset top chain state cache to fork point.
    // ------------------------------------------------------------------------

    const auto top_candidate = state_->height();
    const auto prev_forks = state_->forks();
    const auto prev_version = state_->minimum_block_version();
    state_ = query.get_candidate_chain_state(settings_, fork_point);
    if (!state_)
    {
        close(error::store_integrity);
        return;
    }

    // TODO: this could be moved to deconfirmation.
    const auto next_forks = state_->forks();
    if (prev_forks != next_forks)
    {
        constexpr auto fork_bits = to_bits(sizeof(chain::forks));
        const binary prev{ fork_bits, to_big_endian(prev_forks) };
        const binary next{ fork_bits, to_big_endian(next_forks) };
        LOGN("Forks reverted from ["
            << prev << "] at candidate ("
            << top_candidate << ") to ["
            << next << "] at confirmed ["
            << fork_point << ":" << encode_hash(state_->hash()) << "].");
    }

    // TODO: this could be moved to deconfirmation.
    const auto next_version = state_->minimum_block_version();
    if (prev_version != next_version)
    {
        LOGN("Minimum block version reverted ["
            << prev_version << "] at candidate ("
            << top_candidate << ") to ["
            << next_version << "] at confirmed ["
            << fork_point << ":" << encode_hash(state_->hash()) << "].");
    }

    // Copy candidates from above fork point to top into header tree.
    // ------------------------------------------------------------------------

    auto state = state_;
    for (auto index = add1(fork_point); index <= top_candidate; ++index)
    {
        typename Block::cptr save{};
        if (!get_block(save, index))
        {
            close(error::store_integrity);
            return;
        }

        state.reset(new chain::chain_state{ *state, *save, settings_ });
        cache(save, state);
    }

    // Pop candidates from top to above fork point.
    // ------------------------------------------------------------------------
    for (auto index = top_candidate; index > fork_point; --index)
    {
        LOGN("Deorganizing candidate [" << index << "].");

        if (!query.pop_candidate())
        {
            close(error::store_integrity);
            return;
        }
    }

    // Push confirmed headers from above fork point onto candidate chain.
    // ------------------------------------------------------------------------
    const auto top_confirmed = query.get_top_confirmed();
    for (auto index = add1(fork_point); index <= top_confirmed; ++index)
    {
        if (!query.push_candidate(query.to_confirmed(index)))
        {
            close(error::store_integrity);
            return;
        }
    }
}

TEMPLATE
void CLASS::do_organize(typename Block::cptr& block_ptr,
    const organize_handler& handler) NOEXCEPT
{
        BC_ASSERT(stranded());

        using namespace system;
        auto& query = archive();
        const auto& block = *block_ptr;
        const auto hash = block.hash();
        const auto header = get_header(block);

        // Skip existing/orphan, get state.
        // ------------------------------------------------------------------------

        if (closed())
        {
            handler(network::error::service_stopped, {});
            return;
        }

        const auto it = tree_.find(hash);
        if (it != tree_.end())
        {
            const auto height = it->second.state->height();
            if constexpr (is_same_type<Block, chain::block>)
                handler(error::duplicate_block, height);
            else
                handler(error::duplicate_header, height);

            return;
        }

        // If exists test for prior invalidity.
        const auto link = query.to_header(hash);
        if (!link.is_terminal())
        {
            size_t height{};
            if (!query.get_height(height, link))
            {
                handler(error::store_integrity, {});
                close(error::store_integrity);
                return;
            }

            const auto ec = query.get_header_state(link);
            if (ec == database::error::block_unconfirmable)
            {
                handler(ec, height);
                return;
            }

            if constexpr (is_same_type<Block, chain::block>)
            {
                // Blocks are only duplicates if txs are associated.
                if (ec != database::error::unassociated)
                {
                    handler(error::duplicate_block, height);
                    return;
                }
            }
            else
            {
                handler(error::duplicate_header, height);
                return;
            }
        }

        // Obtains from state_, tree, or store as applicable.
        auto state = get_chain_state(header.previous_block_hash());
        if (!state)
        {
            if constexpr (is_same_type<Block, chain::block>)
                handler(error::orphan_block, {});
            else
                handler(error::orphan_header, {});

            return;
        }

        // Roll chain state forward from previous to current header.
        // ------------------------------------------------------------------------

        const auto prev_forks = state->forks();
        const auto prev_version = state->minimum_block_version();

        // Do not use block parameter here as that override is for tx pool.
        state.reset(new chain::chain_state{ *state, header, settings_ });
        const auto height = state->height();

        // TODO: this could be moved to confirmation.
        const auto next_forks = state->forks();
        if (prev_forks != next_forks)
        {
            constexpr auto fork_bits = to_bits(sizeof(chain::forks));
            const binary prev{ fork_bits, to_big_endian(prev_forks) };
            const binary next{ fork_bits, to_big_endian(next_forks) };
            LOGN("Forked from ["
                << prev << "] to ["
                << next << "] at ["
                << height << ":" << encode_hash(hash) << "].");
        }

        // TODO: this could be moved to confirmation.
        const auto next_version = state->minimum_block_version();
        if (prev_version != next_version)
        {
            LOGN("Minimum block version ["
                << prev_version << "] changed to ["
                << next_version << "] at ["
                << height << ":" << encode_hash(hash) << "].");
        }

        // Validation and currency.
        // ------------------------------------------------------------------------

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

        if (!is_storable(block, height, hash, *state))
        {
            cache(block_ptr, state);
            handler(error::success, height);
            return;
        }

        // Compute relative work.
        // ------------------------------------------------------------------------

        uint256_t work{};
        hashes tree_branch{};
        size_t branch_point{};
        header_links store_branch{};
        if (!get_branch_work(work, branch_point, tree_branch, store_branch, header))
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }

        bool strong{};
        if (!get_is_strong(strong, work, branch_point))
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }

        if (!strong)
        {
            // New top of current weak branch.
            cache(block_ptr, state);
            handler(error::success, height);
            return;
        }

        // Reorganize candidate chain.
        // ------------------------------------------------------------------------

        auto top = state_->height();
        if (top < branch_point)
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }

        // Pop down to the branch point.
        while (top-- > branch_point)
        {
            LOGN("Reorganizing candidate [" << add1(top) << "].");

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
            if (!push(key))
            {
                handler(error::store_integrity, height);
                close(error::store_integrity);
                return;
            }
        }

        // Push new header as top of candidate chain.
        if (push(block_ptr, state->context()).is_terminal())
        {
            handler(error::store_integrity, height);
            close(error::store_integrity);
            return;
        }

        // Reset top chain state cache and notify.
        // ------------------------------------------------------------------------

        const auto point = possible_narrow_cast<height_t>(branch_point);

        if constexpr (is_same_type<Block, chain::block>)
        {
            notify(error::success, chase::block, point);
        }
        else
        {
            // Delay so headers can get current before block download starts.
            if (is_current(header.timestamp()))
                notify(error::success, chase::header, point);
        }

        state_ = state;
        handler(error::success, height);
    }

// private
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::cache(const typename Block::cptr& block_ptr,
    const system::chain::chain_state::ptr& state) NOEXCEPT
{
    tree_.insert({ block_ptr->hash(), { block_ptr, state } });
}

TEMPLATE
system::chain::chain_state::ptr CLASS::get_chain_state(
    const system::hash_digest& hash) const NOEXCEPT
{
    if (!state_)
        return {};

    // Top state is cached because it is by far the most commonly retrieved.
    if (state_->hash() == hash)
        return state_;

    const auto it = tree_.find(hash);
    if (it != tree_.end())
        return it->second.state;

    // Branch forms from a candidate block below top candidate (expensive).
    size_t height{};
    const auto& query = archive();
    if (query.get_height(height, query.to_header(hash)))
        return query.get_candidate_chain_state(settings_, height);

    return {};
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

// ****************************************************************************
// CONSENSUS: branch with greater work causes candidate reorganization.
// Chasers eventually reorganize candidate branch into confirmed if valid.
// ****************************************************************************
TEMPLATE
bool CLASS::get_is_strong(bool& strong, const uint256_t& work,
    size_t branch_point) const NOEXCEPT
{
    strong = false;
    uint256_t candidate_work{};
    const auto& query = archive();
    const auto top = query.get_top_candidate();

    for (auto height = top; height > branch_point; --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_candidate(height)))
            return false;

        // Not strong is candidate work equals or exceeds new work.
        candidate_work += system::chain::header::proof(bits);
        if (candidate_work >= work)
            return true;
    }

    strong = true;
    return true;
}

TEMPLATE
database::header_link CLASS::push(const typename Block::cptr& block_ptr,
    const system::chain::context& context) const NOEXCEPT
{
    auto& query = archive();
    const auto link = query.set_link(*block_ptr, database::context
    {
        system::possible_narrow_cast<flags_t>(context.forks),
        system::possible_narrow_cast<height_t>(context.height),
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

#undef CLASS

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin

#endif
