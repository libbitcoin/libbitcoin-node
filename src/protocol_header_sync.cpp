/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocol_header_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <bitcoin/bitcoin.hpp>

INITIALIZE_TRACK(bc::node::protocol_header_sync);

namespace libbitcoin {
namespace node {

#define NAME "sync_headers"
#define CLASS protocol_header_sync

using namespace bc::config;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr size_t full_headers = 2000;
static const asio::duration one_second(0, 0, 1);

protocol_header_sync::protocol_header_sync(threadpool& pool, p2p&,
    channel::ptr channel, uint32_t minimum_rate, size_t first_height,
    hash_list& headers, const checkpoint::list& checkpoints)
  : protocol_timer(pool, channel, NAME),
    headers_(headers),
    current_second_(0),
    minimum_rate_(minimum_rate),
    start_size_(headers.size()),
    first_height_(first_height),
    target_height_(target(first_height, headers, checkpoints)),
    checkpoints_(checkpoints),
    CONSTRUCT_TRACK(protocol_header_sync, LOG_PROTOCOL)
{
    BITCOIN_ASSERT_MSG(!headers.empty(), "The starting header must be set.");
}

const size_t protocol_header_sync::target(size_t first_height,
    hash_list& headers, const checkpoint::list& checkpoints)
{
    const auto current_block = first_height + headers.size() - 1;
    return checkpoints.empty() ? current_block :
        std::max(checkpoints.back().height(), current_block);
}

void protocol_header_sync::start(event_handler handler)
{
    if (peer_version().start_height < target_height_)
    {
        log::info(LOG_NETWORK)
            << "Start height (" << peer_version().start_height
            << ") below sync target (" << target_height_ << ") from ["
            << authority() << "]";

        handler(error::channel_stopped);
        return;
    }

    auto complete = synchronize(BIND2(headers_complete, _1, handler), 1, NAME);
    protocol_timer::start(one_second, BIND2(handle_event, _1, complete));
    send_get_headers(complete);
}

void protocol_header_sync::send_get_headers(event_handler complete)
{
    if (stopped())
        return;

    BITCOIN_ASSERT_MSG(!headers_.empty(), "The start header must be set.");
    const message::get_headers get_headers{ { headers_.back() }, null_hash };

    SUBSCRIBE3(headers, handle_receive, _1, _2, complete);
    SEND2(get_headers, handle_send, _1, complete);
}

void protocol_header_sync::handle_send(const code& ec, event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure sending get headers to seed [" << authority() << "] "
            << ec.message();
        complete(ec);
    }
}

size_t protocol_header_sync::next_height()
{
    return headers_.size() + first_height_;
}

void protocol_header_sync::rollback()
{
    if (!checkpoints_.empty())
    {
        for (auto it = checkpoints_.rbegin(); it != checkpoints_.rend(); ++it)
        {
            const auto& hash = it->hash();
            auto match = std::find(headers_.begin(), headers_.end(), hash);
            if (match != headers_.end())
            {
                headers_.erase(++match, headers_.end());
                return;
            }
        }
    }

    headers_.resize(1);
}

// We could validate more than this to ensure work is required.
bool protocol_header_sync::merge_headers(const headers& message)
{
    auto previous = headers_.back();
    for (const auto& block: message.elements)
    {
        const auto current = block.hash();
        if (block.previous_block_hash != previous ||
            !checkpoint::validate(current, next_height(), checkpoints_))
        {
            rollback();
            return false;
        }

        previous = current;
        headers_.push_back(current);
    }

    return true;
}

void protocol_header_sync::handle_receive(const code& ec,
    const headers& message, event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure receiving headers from seed ["
            << authority() << "] " << ec.message();
        complete(ec);
        return;
    }

    if (!merge_headers(message))
    {
        log::info(LOG_PROTOCOL)
            << "Failure merging headers from [" << authority() << "]";
        complete(error::previous_block_invalid);
        return;
    }

    if (message.elements.size() >= full_headers)
    {
        log::info(LOG_PROTOCOL)
            << "Synced headers " << next_height() - message.elements.size()
            << "-" << next_height() << " from [" << authority() << "]";
        send_get_headers(complete);
        return;
    }
    
    const auto success = next_height() > target_height_;
    complete(success ? error::success : error::operation_failed);
}

size_t protocol_header_sync::current_rate()
{
    return (headers_.size() - start_size_) / current_second_;
}

// This is fired by the base timer and stop handler.
void protocol_header_sync::handle_event(const code& ec, event_handler complete)
{
    if (ec == error::channel_stopped)
    {
        complete(ec);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        log::warning(LOG_PROTOCOL)
            << "Failure in headers timer for [" << authority() << "] "
            << ec.message();
        complete(ec);
        return;
    }

    // It was a timeout, so one more second has passed.
    ++current_second_;

    // Drop the channel if it falls below the min sync rate.
    if (current_rate() < minimum_rate_)
    {
        log::info(LOG_PROTOCOL)
            << "Header sync rate (" << current_rate()
            << "/sec) from [" << authority() << "] is below minimum ("
            << minimum_rate_ << ").";
        complete(error::channel_timeout);
        return;
    }

    reset_timer();
}

void protocol_header_sync::headers_complete(const code& ec,
    event_handler handler)
{
    // This is the original handler, feedback to the session.
    handler(ec);

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
