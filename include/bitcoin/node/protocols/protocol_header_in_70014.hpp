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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_70014_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_70014_HPP

#include <optional>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_in_70012.hpp>

namespace libbitcoin {
namespace node {

/// Fill compact blocks (bip152 version 2) that extend the confirmed top.
class BCN_API protocol_header_in_70014
  : public protocol_header_in_70012,
    protected network::tracker<protocol_header_in_70014>
{
public:
    typedef std::shared_ptr<protocol_header_in_70014> ptr;

    protocol_header_in_70014(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_header_in_70012(session, channel),
        network::tracker<protocol_header_in_70014>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    using compact_block = network::messages::peer::compact_block;
    using compact_transactions = network::messages::peer::compact_transactions;

    /// A block being filled from its compact block.
    struct fill
    {
        database::header_link link{};
        size_t height{};
        system::hash_digest hash{};
        system::chain::header::cptr header{};
        system::siphash_key key{};
        std::vector<system::chain::short_id::integer> short_ids{};
        system::chain::transaction_cptrs txs{};
        database::tx_links links{};
        std::vector<size_t> missing{};
        std::vector<size_t> unpooled{};
    };

    /// Invoked when initial headers sync is complete.
    void complete() NOEXCEPT override;

    virtual bool handle_receive_compact_block(const code& ec,
        const compact_block::cptr& message) NOEXCEPT;
    virtual bool handle_receive_compact_transactions(const code& ec,
        const compact_transactions::cptr& message) NOEXCEPT;
    virtual void handle_organize_compact(const code& ec, size_t height,
        const compact_block::cptr& message) NOEXCEPT;
    virtual void do_organize_compact(const code& ec,
        const compact_block::cptr& message) NOEXCEPT;
    virtual void handle_submit_compact(const code& ec, size_t index,
        const system::hash_digest& hash, size_t height) NOEXCEPT;
    virtual void do_submit_compact(const code& ec,
        const system::hash_digest& hash, size_t height) NOEXCEPT;

private:
    bool decode(fill& block, const compact_block& message) NOEXCEPT;
    bool scan(fill& block) NOEXCEPT;
    bool to_hashes(system::hashes& txids, system::hashes& wtxids,
        const fill& block) NOEXCEPT;
    bool is_malleated64(const fill& block) NOEXCEPT;

    void collect(const compact_block& message,
        const database::header_link& link) NOEXCEPT;
    void request() NOEXCEPT;
    void identify() NOEXCEPT;
    void resolve() NOEXCEPT;

    // These are protected by strand.
    bool compact_{};
    std::optional<fill> fill_{};
    database::header_link evidence_link_{};
    system::hashes evidence_{};
};

} // namespace node
} // namespace libbitcoin

#endif
