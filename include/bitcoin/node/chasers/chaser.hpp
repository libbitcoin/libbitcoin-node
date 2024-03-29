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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Abstract base chaser for thread safe chain state management classes.
/// Chasers impose order on blockchain/pool construction as necessary.
/// Each chaser operates on its own strand, implemented here, allowing
/// concurrent chaser operations to the extent that threads are available.
/// Events are passed between chasers using the full_node shared notifier.
/// Unlike protocols chasers can stop the node.
class BCN_API chaser
  : public network::reporter
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser);

    /// Should be called from node strand.
    virtual code start() NOEXCEPT = 0;

protected:
    /// Abstract base class protected construct.
    chaser(full_node& node) NOEXCEPT;

    /// Binders.
    /// -----------------------------------------------------------------------

    /// Bind a method (use BIND).
    template <class Derived, typename Method, typename... Args>
    auto bind(Method&& method, Args&&... args) NOEXCEPT
    {
        return BIND_THIS(method, args);
    }

    /// Post a method to channel strand (use POST).
    template <class Derived, typename Method, typename... Args>
    auto post(Method&& method, Args&&... args) NOEXCEPT
    {
        return boost::asio::post(strand(), BIND_THIS(method, args));
    }

    /// Close.
    /// -----------------------------------------------------------------------

    /// Close the node after logging the code.
    void close(const code& ec) const NOEXCEPT;

    /// Node threadpool is stopped and may still be joining.
    bool closed() const NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Call from chaser start methods (requires node strand).
    code subscribe_events(event_handler&& handler) NOEXCEPT;

    /// Set event (does not require node strand).
    void notify(const code& ec, chase event_, event_link value) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Node configuration settings.
    const node::configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// The chaser's strand.
    network::asio::strand& strand() NOEXCEPT;

    /// True if the current thread is on the chaser strand.
    bool stranded() const NOEXCEPT;

    /// Header timestamp is within configured span from current time.
    bool is_current(uint32_t timestamp) const NOEXCEPT;

private:
    // These are thread safe (mostly).
    full_node& node_;
    network::asio::strand strand_;
};

#define SUBSCRIBE_EVENTS(method, ...) \
    subscribe_events(BIND(method, __VA_ARGS__))

} // namespace node
} // namespace libbitcoin

#endif
