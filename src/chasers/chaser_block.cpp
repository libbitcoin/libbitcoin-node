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
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser_organize.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

using namespace system::chain;

chaser_block::chaser_block(full_node& node) NOEXCEPT
  : chaser_organize<block>(node)
{
}

const header& chaser_block::get_header(const block& block) const NOEXCEPT
{
    return block.header();
}

bool chaser_block::get_block(system::chain::block::cptr& out,
    size_t index) const NOEXCEPT
{
    out = archive().get_block(archive().to_candidate(index));
    return !is_null(out);
}

// Block validations are bypassed when under checkpoint.
code chaser_block::validate(const system::chain::block& block,
    const chain_state& state) const NOEXCEPT
{
    code ec{ error::success };
    const auto& header = block.header();

    // block.check does not roll up to header.check.
    if ((ec = header.check(
        settings().timestamp_limit_seconds,
        settings().proof_of_work_limit,
        settings().scrypt_proof_of_work)))
        return ec;

    // block.accept does not roll up to header.accept.
    if ((ec = header.accept(state.context())))
        return ec;

    // Under height is sufficient validation for checkpoint here.
    if (checkpoint::is_under(settings().checkpoints, state.height()))
        return ec;

    if ((ec = block.check()))
        return ec;

    if ((ec = block.check(state.context())))
        return ec;

    // Populate prevouts from self/tree.
    // Metadata population is only for confirmation, not required.
    populate(block);

    if (!archive().populate(block))
        return network::error::protocol_violation;

    if ((ec = block.accept(state.context(),
        settings().subsidy_interval_blocks,
        settings().initial_subsidy())))
        return ec;

    return block.connect(state.context());
}

// Blocks are accumulated following genesis, not cached until current.
bool chaser_block::is_storable(const system::chain::block&,
    const chain_state&) const NOEXCEPT
{
    return true;
}

void chaser_block::set_prevout(const input& input) const NOEXCEPT
{
    const auto& point = input.point();

    // Scan all blocks for matching tx (linear :/)
    std::for_each(tree().begin(), tree().end(), [&](const auto& item) NOEXCEPT
    {
        const auto& txs = *item.second.block->transactions_ptr();
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

} // namespace database
} // namespace libbitcoin
