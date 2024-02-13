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
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {
    
using namespace network;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser(node),
    currency_window_(node.node_settings().currency_window()),
    tracker<chaser_header>(node.log)
{
    subscribe(std::bind(&chaser_header::handle_event, this, _1, _2, _3));
}

// private
void chaser_header::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_header::do_handle_event, this, ec, event_, value));
}

// private
void chaser_header::do_handle_event(const code& ec, chase event_, link) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    if (ec)
        return;

    switch (event_)
    {
        case chase::start:
            // TODO: initialize header tree from store.
            break;
        default:
            return;
    }
}

// private
bool chaser_header::is_current(const header& header) const NOEXCEPT
{
    const auto time = wall_clock::from_time_t(header.timestamp());
    const auto current = wall_clock::now() - currency_window_;
    return time >= current;
}

// private
// ****************************************************************************
// CONSENSUS: tree branch with greater work causes candidate re/organization.
// Chasers eventually re/organize candidate branch into confirmed if valid.
// ****************************************************************************
bool chaser_header::is_strong(const header& header) const NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    // TODO: Compute branch proof above branch point.
    const uint256_t branch = header.proof();

    // TODO: Compute candidate proof above branch point to strong.
    // If candidate proof ever exceeds branch proof then return false.
    // This is a short-circuiting optimization to minimize store reads.
    // Since this strand is the only place where the candidate chain changes
    // the strong computation here is protected by the strand.
    const uint256_t candidate{};

    // TODO: Each new header is currently strong (assumes no branching).
    return branch > candidate;
}

void chaser_header::store(const header::cptr& header) NOEXCEPT
{
    // Full accept performed in protocol (for concurrency).
    boost::asio::post(strand(),
        std::bind(&chaser_header::do_store, this, header));
}

// private
void chaser_header::do_store(const header::cptr& header) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    // If false we assume the parent is deeper on the stack, since protocol
    // connected, and therefore this header is as well (assumes linear).
    if (!chain_.empty() && chain_.back()->hash() ==
        header->previous_block_hash())
    {
        chain_.push_back(header);

        if (is_current(*header) && is_strong(*header))
        {
            // TODO: reorganize current branch into the candidate chain.
            // TODO: store and pop tree branch (chain_).
            chain_.clear();

            notify(error::success, chase::checked, { 42_i32 });
        }
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
