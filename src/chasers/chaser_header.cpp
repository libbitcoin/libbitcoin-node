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

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser_organize.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {
    
using namespace system::chain;

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser_organize<header>(node),
    milestone_(config().bitcoin.milestone)
{
}

code chaser_header::start() NOEXCEPT
{
    if (!initialize_milestone())
        return fault(database::error::integrity);

    return chaser_organize<header>::start();
}

const header& chaser_header::get_header(const header& header) const NOEXCEPT
{
    return header;
}

bool chaser_header::get_block(header::cptr& out, size_t height) const NOEXCEPT
{
    const auto& query = archive();
    out = query.get_header(query.to_candidate(height));
    return !is_null(out);
}

code chaser_header::duplicate(size_t& height,
    const system::hash_digest& hash) const NOEXCEPT
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
        // database::error::unknown_state
        // database::error::unvalidated
        const auto ec = query.get_header_state(id);

        // All header states are duplicates, one implies fail.
        if (ec == database::error::block_unconfirmable)
        {
            height = query.get_height(id);
            return ec;
        }

        return error::duplicate_header;
    }

    return error::success;
}

code chaser_header::validate(const header& header,
    const chain_state& state) const NOEXCEPT
{
    code ec{};

    // header.check is never bypassed.
    if ((ec = header.check(
        settings().timestamp_limit_seconds,
        settings().proof_of_work_limit,
        settings().forks.scrypt_proof_of_work)))
        return ec;

    // header.accept is never bypassed.
    if ((ec = header.accept(state.context())))
        return ec;

    // This prevents a long unconfirmable header chain with an early
    // unconfirmable from reinitiating a long validation chain before hitting
    // the invalidation again. This is more likely the case of a bug than IRL.
    ////const auto& query = archive();
    ////ec = query.get_header_state(query.to_header(header.hash()));
    ////if (ec == database::error::block_unconfirmable)
    ////    return ec;

    return system::error::block_success;
}

// Storable methods (private).
// ----------------------------------------------------------------------------

bool chaser_header::is_storable(const chain_state& state) const NOEXCEPT
{
    return is_checkpoint(state) || is_milestone(state)
        || (is_current(state) && is_hard(state));
}

bool chaser_header::is_checkpoint(const chain_state& state) const NOEXCEPT
{
    return checkpoint::is_at(settings().checkpoints, state.height());
}

bool chaser_header::is_milestone(const chain_state& state) const NOEXCEPT
{
    return milestone_.equals(state.hash(), state.height());
}

bool chaser_header::is_current(const chain_state& state) const NOEXCEPT
{
    return chaser::is_current(state.timestamp());
}

bool chaser_header::is_hard(const chain_state& state) const NOEXCEPT
{
    return state.cumulative_work() >= settings().minimum_work;
}

// Milestone methods.
// ----------------------------------------------------------------------------

// private
bool chaser_header::initialize_milestone() NOEXCEPT
{
    active_milestone_height_ = zero;
    if (is_zero(milestone_.height()) ||
        milestone_.hash() == system::null_hash)
        return true;

    const auto& query = archive();
    const auto link = query.to_candidate(milestone_.height());
    if (link.is_terminal())
        return true;

    const auto hash = query.get_header_key(link);
    if (hash == system::null_hash)
        return false;

    if (hash == milestone_.hash())
        active_milestone_height_ = milestone_.height();

    return true;
}

bool chaser_header::is_under_milestone(size_t height) const NOEXCEPT
{
    return height <= active_milestone_height_;
}

void chaser_header::update_milestone(const system::chain::header& header,
    size_t height, size_t branch_point) NOEXCEPT
{
    if (milestone_.equals(header.get_hash(), height))
    {
        active_milestone_height_ = height;
        return;
    }

    // Use pointer to avoid const/copy.
    auto previous = &header.previous_block_hash();

    // Scan branch for milestone match.
    for (auto it = tree().find(*previous); it != tree().end();
        it = tree().find(*previous))
    {
        const auto index = it->second.state->height();
        if (milestone_.equals(it->second.state->hash(), index))
        {
            active_milestone_height_ = index;
            return;
        }

        const auto& next = get_header(*it->second.block);
        previous = &next.previous_block_hash();
    }

    // The current active milestone is necessarily on the candidate branch.
    // New branch doesn't have milestone and reorganizes the branch with it.
    // Can retain a milestone at the branch point (below its definition).
    if (active_milestone_height_ > branch_point)
        active_milestone_height_ = branch_point;
}

} // namespace node
} // namespace libbitcoin

// bip90 prevents bip34/65/66 activation oscillations

// Forks (default configuration).
// Forked from [00000000000000100000000010001011] to [00000000000000100000000010000011] at [ 91842:00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec]
// Forked from [00000000000000100000000010000011] to [00000000000000100000000010001011] at [ 91843:0000000000016ca756e810d44aee6be7eabad75d2209d7f4542d1fd53bafc984]
// Forked from [00000000000000100000000010001011] to [00000000000000100000000010000011] at [ 91880:00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721]
// Forked from [00000000000000100000000010000011] to [00000000000000100000000010001011] at [ 91881:00000000000cf6204ce82deed75b050014dc57d7bb462d7214fcc4e59c48d66d]
// Forked from [00000000000000100000000010001011] to [00000000000000100000000010001111] at [173805:00000000000000ce80a7e057163a4db1d5ad7b20fb6f598c9597b9665c8fb0d4]
// Forked from [00000000000000100000000010001111] to [00000000000000100000000010010111] at [227931:000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8]
// Forked from [00000000000000100000000010010111] to [00000000000000100000000010110111] at [363725:00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931]
// Forked from [00000000000000100000000010110111] to [00000000000000100000000011110111] at [388381:000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0]
// Forked from [00000000000000100000000011110111] to [00000000000000100000011111110111] at [419328:000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5]
// Forked from [00000000000000100000011111110111] to [00000000000000100011111111110111] at [481824:0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893]

// Minimum block versions (bip90 disabled).
// Minimum block version [1] changed to [2] at [227931:000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8]
// Minimum block version [2] changed to [3] at [363725:00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931]
// Minimum block version [3] changed to [4] at [388381:000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0]