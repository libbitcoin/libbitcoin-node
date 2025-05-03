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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_HPP

#include <bitcoin/database.hpp>
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

    /// Override to capture non-blocking stopping.
    virtual void stopping(const code& ec) NOEXCEPT;

    /// Override to capture blocking stop.
    virtual void stop() NOEXCEPT;
    
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

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Node threadpool is stopped and may still be joining.
    virtual bool closed() const NOEXCEPT;

    /// Network connections are suspended (incoming and/or outgoing).
    virtual bool suspended() const NOEXCEPT;

    /// Suspend all existing and future network connections.
    /// A race condition could result in an unsuspended connection.
    virtual code fault(const code& ec)  NOEXCEPT;

    /// Resume all network connections.
    virtual void resume() NOEXCEPT;

    /// Snapshot the store, suspends and resumes network.
    virtual code snapshot(const store::event_handler& handler) NOEXCEPT;

    /// Reset store disk full condition.
    virtual code reload(const store::event_handler& handler) NOEXCEPT;

    /// Get reorganization lock.
    virtual lock get_reorganization_lock() const NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Call from chaser start methods (requires node strand).
    virtual object_key subscribe_events(event_notifier&& handler) NOEXCEPT;

    /// Set event (does not require node strand).
    virtual void notify(const code& ec, chase event_,
        event_value value) const NOEXCEPT;

    /// Set event to one subscriber (does not require node strand).
    virtual void notify_one(object_key key, const code& ec, chase event_,
        event_value value) const NOEXCEPT;

    /// Strand.
    /// -----------------------------------------------------------------------

    /// The chaser's strand (on the network threadpool).
    virtual network::asio::strand& strand() NOEXCEPT;

    /// True if the current thread is on the chaser strand.
    virtual bool stranded() const NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Node configuration settings.
    const node::configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Top candidate is within configured span from current time.
    bool is_current() const NOEXCEPT;

    /// Header timestamp is within configured span from current time.
    bool is_current(uint32_t timestamp) const NOEXCEPT;

    /// Header's timestamp is within configured span from current time.
    bool is_current(const database::header_link& link) const NOEXCEPT;

    /// The height is at or below the top checkpoint.
    bool is_under_checkpoint(size_t height) const NOEXCEPT;

    /// The height of the top checkpoint.
    size_t checkpoint() const NOEXCEPT;

    /// Position (requires strand).
    /// -----------------------------------------------------------------------

    size_t position() const NOEXCEPT;
    void set_position(size_t height) NOEXCEPT;

private:
    // These are thread safe (mostly).
    full_node& node_;
    network::asio::strand strand_;
    const size_t top_checkpoint_height_;

    // These are protected by strand.
    size_t position_{};
};

#define SUBSCRIBE_EVENTS(method, ...) \
    subscribe_events(BIND(method, __VA_ARGS__))

#define PARALLEL(method, ...) \
    boost::asio::post(threadpool_.service(), BIND(method, __VA_ARGS__));

} // namespace node
} // namespace libbitcoin

#endif
