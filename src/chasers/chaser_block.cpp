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
#include <bitcoin/node/chasers/chaser_block.hpp>

#include <algorithm>
#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser_organize.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {
    
using namespace system;
using namespace system::chain;

chaser_block::chaser_block(full_node& node) NOEXCEPT
  : chaser_organize<block>(node),
    node_witness_(node.config().network.witness_node())
{
}

const header& chaser_block::get_header(const block& block) const NOEXCEPT
{
    return block.header();
}

bool chaser_block::get_block(block::cptr& out,
    const header_link& link) const NOEXCEPT
{
    const auto& query = archive();
    out = query.get_block(link, node_witness_);
    return !is_null(out);
}

code chaser_block::duplicate(size_t& height,
    const system::hash_digest& hash) const NOEXCEPT
{
    height = max_size_t;
    const auto& query = archive();
    const auto id = query.to_header(hash);
    if (!id.is_terminal())
    {
        // database::error::unassociated
        // database::error::block_unconfirmable
        // database::error::block_confirmable
        // database::error::block_valid
        // database::error::unknown_state
        // database::error::unvalidated
        const auto ec = query.get_block_state(id);

        // Most header states are duplicates, one implies fail.
        if (ec == database::error::block_unconfirmable)
        {
            height = query.get_height(id);
            return ec;
        }

        // unassociated is only non-duplicate.
        if (ec != database::error::unassociated)
        {
            height = query.get_height(id);
            return error::duplicate_block;
        }
    }

    return error::success;
}

code chaser_block::validate(const block& block,
    const chain_state& state) const NOEXCEPT
{
    code ec{};
    const auto& header = block.header();
    const auto& setting = settings();
    const auto ctx = state.context();

    // header.check is never bypassed.
    // block.check does not invoke header.check.
    if ((ec = header.check(
        setting.timestamp_limit_seconds,
        setting.proof_of_work_limit,
        setting.forks.scrypt_proof_of_work)))
        return ec;

    // header.accept is never bypassed.
    // block.accept does not invoke header.accept.
    if ((ec = header.accept(ctx)))
        return ec;

    if (is_under_checkpoint(state.height()))
    {
        // Only identity is required under checkpoint.
        if (((ec = block.identify())) || ((ec = block.identify(ctx))))
            return ec;
    }
    else
    {
        // Identity is not assured if invalid (but is not required).
        if (((ec = block.check())) || ((ec = block.check(ctx))))
            return ec;

        if (!populate(block))
            return system::error::missing_previous_output;

        if ((ec = block.accept(ctx,
            setting.subsidy_interval_blocks,
            setting.initial_subsidy())))
            return ec;

        if ((ec = block.connect(ctx)))
            return ec;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: chaser_validate bypasses set filter body, prevouts, block_valid.
    // These and notify(chase::valid) could be set here in chaser_block but
    // blocks are populated from the block tree and therefore there is not yet
    // tx metadata for all prevouts. So presently blocks first does not proceed
    // to confirmation following this block validation and header organization.
    ///////////////////////////////////////////////////////////////////////////
    //// None of this can be done because block is not archived.
    ////
    ////auto& query = archive();
    ////if (!query.set_prevouts(link, block))
    ////    return error::validate6;
    ////
    ////if (!query.set_filter_body(link, block))
    ////    return error::validate7;
    ////
    ////// After set_prevouts and set_filter_body.
    ////if (!bypass && !query.set_block_valid(link, block.fees()))
    ////    return error::validate8;
    ////
    ////notify(error::success, chase::valid, state.height());
    ///////////////////////////////////////////////////////////////////////////

    return system::error::block_success;
}

bool chaser_block::is_storable(const chain_state&) const NOEXCEPT
{
    return true;
}

// Milestone methods.
// ----------------------------------------------------------------------------

bool chaser_block::is_under_milestone(size_t) const NOEXCEPT
{
    return false;
}

bool chaser_block::update_milestone(const header&, size_t, size_t) NOEXCEPT
{
    return false;
}

// Populate methods (private).
// ----------------------------------------------------------------------------

void chaser_block::set_prevout(const input& input) const NOEXCEPT
{
    const auto& point = input.point();
    if (input.prevout || point.is_null())
        return;

    // Scan all tree blocks for matching tx (linear :/ but legacy scenario)
    std::ranges::for_each(tree(), [&](const auto& item) NOEXCEPT
    {
        const auto& txs = item.second->transactions_ptr();
        const auto it = std::ranges::find_if(*txs, [&](const auto& tx) NOEXCEPT
        {
            return tx->hash(false) == point.hash();
        });

        if (it != txs->end())
        {
            const auto& tx = **it;
            const auto& outs = *tx.outputs_ptr();
            if (point.index() < outs.size())
            {
                // prevout is mutable so can be set on a const object.
                input.prevout = outs.at(point.index());
                return;
            }
        }
    });
}

// Populate prevouts from self/tree/store (without metadata).
bool chaser_block::populate(const block& block) const NOEXCEPT
{
    block.populate();

    const auto& ins = *block.inputs_ptr();
    std::ranges::for_each(ins, [&](const auto& in) NOEXCEPT
    {
        set_prevout(*in);
    });

    return archive().populate_without_metadata(block);
}

} // namespace node
} // namespace libbitcoin
