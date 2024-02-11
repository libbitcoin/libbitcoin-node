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
#include <bitcoin/node/full_node.hpp>

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/sessions/sessions.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

using namespace network;
using namespace std::placeholders;

// p2p::strand() as it is non-virtual (safe to call from constructor).
full_node::full_node(query& query, const configuration& configuration,
    const logger& log) NOEXCEPT
  : p2p(configuration.network, log),
    config_(configuration),
    query_(query),
    event_subscriber_(strand())
{
}

// Sequences.
// ----------------------------------------------------------------------------

void full_node::start(result_handler&& handler) NOEXCEPT
{
    if (!query_.is_initialized())
    {
        handler(error::store_uninitialized);
        return;
    }

    // Base (p2p) invokes do_start().
    p2p::start(std::move(handler));
}

void full_node::do_start(const result_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "full_node");

    const auto ec = create_chasers();

    if (ec)
    {
        handler(ec);
        return;
    }

    p2p::do_start(handler);
}

void full_node::run(result_handler&& handler) NOEXCEPT
{
    // Base (p2p) invokes do_run().
    p2p::run(std::move(handler));
}

void full_node::do_run(const result_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "full_node");

    if (closed())
    {
        handler(network::error::service_stopped);
        return;
    }

    // Do stuff here.

    p2p::do_run(handler);
}

void full_node::close() NOEXCEPT
{
    // Base (p2p) invokes do_close().
    p2p::close();
}

void full_node::do_close() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "full_node");

    stop_chasers();
    p2p::do_close();
    delete_chasers();
}

// Chasers.
// ----------------------------------------------------------------------------

code full_node::create_chasers() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "full_node");

    // Create and subscribe all chasers.
    chaser_header_ = std::make_unique<chaser_header>(*this);
    chaser_check_ = std::make_unique<chaser_check>(*this);
    chaser_connect_ = std::make_unique<chaser_connect>(*this);
    chaser_confirm_ = std::make_unique<chaser_confirm>(*this);
    chaser_transaction_ = std::make_unique<chaser_transaction>(*this);
    chaser_candidate_ = std::make_unique<chaser_candidate>(*this);

    // Post start event to all chasers.
    event_subscriber_.notify(error::success, chaser::chase::start);
    return error::success;
}

void full_node::stop_chasers() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "full_node");

    event_subscriber_.stop(network::error::service_stopped,
        chaser::chase::stop);
}

// These should be reset upon destruct, which could only follow close(), which
// ensures that the threadpool is coalesced. Yet without explicit delete, msvc
// asserts on process termination.
void full_node::delete_chasers() NOEXCEPT
{
    chaser_header_.reset();
    chaser_check_.reset();
    chaser_connect_.reset();
    chaser_confirm_.reset();
    chaser_transaction_.reset();
    chaser_candidate_.reset();
}

// Properties.
// ----------------------------------------------------------------------------

full_node::query& full_node::archive() const NOEXCEPT
{
    return query_;
}

const configuration& full_node::config() const NOEXCEPT
{
    return config_;
}

// protected
chaser::event_subscriber& full_node::event_subscriber() NOEXCEPT
{
    return event_subscriber_;
}

// Session attachments.
// ----------------------------------------------------------------------------

session_manual::ptr full_node::attach_manual_session() NOEXCEPT
{
    return p2p::attach<node::session_manual>(*this);
}

session_inbound::ptr full_node::attach_inbound_session() NOEXCEPT
{
    return p2p::attach<node::session_inbound>(*this);
}

session_outbound::ptr full_node::attach_outbound_session() NOEXCEPT
{
    return p2p::attach<node::session_outbound>(*this);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
