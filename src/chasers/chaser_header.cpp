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
  : chaser_organize<header>(node)
{
}

const header& chaser_header::get_header(const header& header) const NOEXCEPT
{
    return header;
}

bool chaser_header::get_block(system::chain::header::cptr& out,
    size_t index) const NOEXCEPT
{
    out = archive().get_header(archive().to_candidate(index));
    return !is_null(out);
}

// Header check/accept are not bypassed when under checkpoint/milestone.
// This is because checkpoint would be reliant on height alone and milestone
// would have no reliance because its passage is not required.
code chaser_header::validate(const system::chain::header& header,
    const chain_state& state) const NOEXCEPT
{
    code ec{ error::success };

    if ((ec = header.check(
        settings().timestamp_limit_seconds,
        settings().proof_of_work_limit,
        settings().forks.scrypt_proof_of_work)))
        return ec;

    if ((ec = header.accept(state.context())))
        return ec;

    return system::error::success;
}

// Cache valid headers until storable.
bool chaser_header::is_storable(const system::chain::header& header,
    const chain_state& state) const NOEXCEPT
{
    return
        checkpoint::is_at(settings().checkpoints, state.height()) ||
        settings().milestone.equals(state.hash(), state.height()) ||
        (is_current(header.timestamp()) &&
            state.cumulative_work() >= settings().minimum_work);
}

} // namespace database
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