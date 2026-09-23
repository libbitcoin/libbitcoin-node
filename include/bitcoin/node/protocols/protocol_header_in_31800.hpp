/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {

/// Synchronize the peer's header branch, validating and discarding headers
/// until proven (a checkpoint, or exhausted, current and at minimum work).
/// Then get it again and archive as verified against sampled hashes.
class BCN_API protocol_header_in_31800
  : public node::protocol_peer,
    protected network::tracker<protocol_header_in_31800>
{
public:
    typedef std::shared_ptr<protocol_header_in_31800> ptr;

    protocol_header_in_31800(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_peer(session, channel),
        network::tracker<protocol_header_in_31800>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    using chain_state = system::chain::chain_state;
    using headers = network::messages::peer::headers;
    using inventory = network::messages::peer::inventory;
    using get_headers = network::messages::peer::get_headers;

    virtual bool handle_receive_inventory(const code& ec,
        const inventory::cptr& message) NOEXCEPT;
    virtual bool handle_receive_headers(const code& ec,
        const headers::cptr& message) NOEXCEPT;
    virtual void handle_organize(const code& ec, size_t height,
        const system::chain::header::cptr& header_ptr) NOEXCEPT;
    virtual void complete() NOEXCEPT;

    // This is protected by strand.
    bool subscribed{};

private:

    void synchronize(const headers& message, bool full) NOEXCEPT;
    void collect(const headers& message, bool full) NOEXCEPT;
    bool restart(const system::hash_digest& previous) NOEXCEPT;
    void sample(const system::hash_digest& hash) NOEXCEPT;
    void prove() NOEXCEPT;
    void finish() NOEXCEPT;

    get_headers create_get_headers() const NOEXCEPT;
    get_headers create_get_headers(
        const system::hash_digest& last) const NOEXCEPT;
    get_headers create_get_headers(
        system::hashes&& start_hashes) const NOEXCEPT;

    // These are protected by strand.
    bool archiving_{};
    size_t top_{};
    size_t index_{};
    size_t height_{};
    size_t milestone_{};
    system::hashes samples_{};
    system::hash_digest previous_{};
    system::chain::header_cptrs buffer_{};
    size_t interval_{ network::messages::peer::max_get_headers };
    chain_state::cptr state_{};
};

} // namespace node
} // namespace libbitcoin

#endif
