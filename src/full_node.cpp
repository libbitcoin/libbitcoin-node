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
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

using namespace network;
using namespace std::placeholders;

// TODO: replace channel_heartbeat.
full_node::full_node(query& query, const configuration& configuration,
    const logger& log) NOEXCEPT
  : p2p(configuration.network, log),
    config_(configuration),
    query_(query),
    poll_subscriber_(strand()),
    poll_timer_(std::make_shared<deadline>(log, strand(),
        configuration.network.channel_heartbeat()))
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

    p2p::start(std::move(handler));
}

// Base (p2p) invokes do_run() override.
////void full_node::run(result_handler&& handler) NOEXCEPT
////{
////    p2p::run(std::move(handler));
////}

void full_node::do_run(const result_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "timer");

    if (closed())
    {
        handler(network::error::service_stopped);
        return;
    }

    poll_timer_->start(std::bind(&full_node::poll, this, _1));
    p2p::do_run(handler);
}

void full_node::poll(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "subscriber");

    if (closed() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        LOGF("Poll timer error, " << ec.message());
        return;
    }

    // TODO: get each channel bytes.
    poll_subscriber_.notify(error::success, 42_size);

    // TODO: stop slow channel(s).
    object_key key{};
    poll_subscriber_.notify_one(key, error::slow_channel, {});
}

void full_node::do_close() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "timer");
    poll_timer_->stop();
    p2p::do_close();
}

// Subscriptions.
// ----------------------------------------------------------------------------

void full_node::subscribe_poll(object_key key,
    poll_notifier&& handler) NOEXCEPT
{
    // Public methods can complete on caller thread.
    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    boost::asio::post(strand(),
        std::bind(&full_node::do_subscribe_poll,
            this, key, std::move(handler)));
}

// private
void full_node::do_subscribe_poll(object_key key,
    const poll_notifier& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    poll_subscriber_.subscribe(move_copy(handler), key);
}

void full_node::unsubscribe_poll(object_key key) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&full_node::do_unsubscribe_poll, this, key));
}

void full_node::do_unsubscribe_poll(object_key key) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    poll_subscriber_.notify_one(key, network::error::desubscribed, {});
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

// Session attachments.
// ----------------------------------------------------------------------------

session_manual::ptr full_node::attach_manual_session() NOEXCEPT
{
    return p2p::attach<mixin<session_manual>>(*this);
}

session_inbound::ptr full_node::attach_inbound_session() NOEXCEPT
{
    return p2p::attach<mixin<session_inbound>>(*this);
}

session_outbound::ptr full_node::attach_outbound_session() NOEXCEPT
{
    return p2p::attach<mixin<session_outbound>>(*this);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
