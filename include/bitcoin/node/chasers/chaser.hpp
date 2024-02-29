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

#include <functional>
#include <variant>
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
/// Notifications are bounced from sink (e.g. chaser) to its strand, and
/// notify bounces from source (e.g. chaser) to network strand.
/// Unlike protocols chasers can stop the node.
/// Logging is currently disabled so that logging is kept to the protocols.
class BCN_API chaser
////  : public network::reporter
{
public:
    enum class chase
    {
        /// A new strong branch exists (strong height_t).
        /// Issued by 'block' and handled by 'confirm'.
        /// The block chaser works with the blocks-first protocol.
        /// Bocks first performs header/checked/connected stages.
        block,

        /// A new strong branch exists (strong height_t).
        /// Issued by 'header' and handled by 'check'.
        /// The block chaser works with the header-first protocol.
        header,

        /// New unassociated blocks exist in the strong branch.
        unassociated,

        /// A block has been downloaded, checked and stored (header_t).
        /// Issued by 'check' and handled by 'connect'.
        checked,
        unchecked,

        /// A branch has been connected (header_t|height_t).
        /// Issued by 'connect' and handled by 'confirm'.
        connected,
        unconnected,

        /// A branch has been confirmed (fork header_t|height_t).
        /// Issued by 'confirm' and handled by 'transaction'.
        confirmed,
        unconfirmed,

        /// A new transaction has been added to the pool (transaction_t).
        /// Issued by 'transaction' and handled by 'candidate'.
        transaction,

        /// A new candidate block has been created (?).
        /// Issued by 'candidate' and handled by miners.
        candidate,

        /// Service is stopping (accompanied by error::service_stopped).
        stop
    };

    using height_t = database::height_link::integer;
    using header_t = database::header_link::integer;
    using transaction_t = database::tx_link::integer;
    using flags_t = database::context::flag::integer;

    typedef std::function<void(const code&, size_t)> organize_handler;
    typedef std::variant<uint32_t, uint64_t> link;
    typedef network::subscriber<chase, link> event_subscriber;
    typedef event_subscriber::handler event_handler;

    typedef database::store<database::map> store;
    typedef database::query<store> query;
    DELETE_COPY_MOVE(chaser);

    /// Synchronously subscribe to notify and asynchronously initialize state.
    virtual code start() NOEXCEPT = 0;

protected:
    chaser(full_node& node) NOEXCEPT;
    ~chaser() NOEXCEPT;

    /// Node threadpool is stopped and may still be joining.
    bool closed() const NOEXCEPT;

    /// Node configuration settings.
    const node::configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// The chaser's strand.
    network::asio::strand& strand() NOEXCEPT;

    /// True if the current thread is on the chaser strand.
    bool stranded() const NOEXCEPT;

    /// True if the current thread is on the node strand.
    bool node_stranded() const NOEXCEPT;

    /// Subscribe to chaser events.
    code subscribe(event_handler&& handler) NOEXCEPT;

    /// Set chaser event (does not require network strand).
    void notify(const code& ec, chase event_, link value) NOEXCEPT;

private:
    void do_notify(const code& ec, chase event_, link value) NOEXCEPT;

    // These are thread safe (mostly).
    full_node& node_;
    network::asio::strand strand_;

    // This is protected by the network strand.
    event_subscriber& subscriber_;
};

} // namespace node
} // namespace libbitcoin

#endif
