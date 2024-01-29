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
#include <bitcoin/node/chasers/block_chaser.hpp>

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    
// TODO: replace channel_heartbeat.
block_chaser::block_chaser(full_node& node) NOEXCEPT
  : node_(node),
    strand_(node.service().get_executor()),
    subscriber_(strand_),
    timer_(std::make_shared<network::deadline>(node.log, strand_,
        node.network_settings().channel_heartbeat())),
    reporter(node.log),
    tracker<block_chaser>(node.log)
{
}

block_chaser::~block_chaser() NOEXCEPT
{
    BC_ASSERT_MSG(stopped(), "The block chaser was not stopped.");
    if (!stopped()) { LOGF("~block_chaser is not stopped."); }
}

void block_chaser::start(network::result_handler&& handler) NOEXCEPT
{
    if (!stopped())
    {
        handler(network::error::operation_failed);
        return;
    }

    timer_->start([this](const code& ec) NOEXCEPT
    {
        BC_ASSERT_MSG(stranded(), "strand");

        if (stopped())
            return;

        if (ec)
        {
            LOGF("Chaser timer fail, " << ec.message());
            stop();
            return;
        }

        // TODO: collect performance.
        ////stop(network::error::channel_expired);
    });

    stopped_.store(false);
    handler(network::error::success);
}

void block_chaser::stop() NOEXCEPT
{
    stopped_.store(true);

    // The block_chaser can be deleted once threadpool joins after this call.
    boost::asio::post(strand_,
        std::bind(&block_chaser::do_stop, this));
}

block_chaser::object_key block_chaser::subscribe(notifier&& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    const auto key = create_key();
    subscriber_.subscribe(std::move(handler), key);
    return key;
}

// TODO: closing channel notifies itself to desubscribe.
bool block_chaser::notify(object_key key) NOEXCEPT
{
    return subscriber_.notify_one(key, network::error::success);
}

bool block_chaser::stopped() const NOEXCEPT
{
    return stopped_.load();
}

bool block_chaser::stranded() const NOEXCEPT
{
    return strand_.running_in_this_thread();
}

// private
block_chaser::object_key block_chaser::create_key() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");

    if (is_zero(++keys_))
    {
        BC_ASSERT_MSG(false, "overflow");
        LOGF("Chaser object overflow.");
    }

    return keys_;
}

// private
void block_chaser::do_stop() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");

    timer_->stop();
    subscriber_.stop(network::error::service_stopped);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
