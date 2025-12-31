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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_ELECTRUM_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_ELECTRUM_HPP

#include <memory>
#include <unordered_map>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_electrum
  : public node::protocol_rpc<channel_electrum>,
    protected network::tracker<protocol_electrum>
{
public:
    typedef std::shared_ptr<protocol_electrum> ptr;
    using rpc_interface = interface::electrum;

    inline protocol_electrum(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_rpc<channel_electrum>(session, channel, options),
        network::tracker<protocol_electrum>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers (blockchain).
    void handle_blockchain_block_header(const code& ec,
        rpc_interface::blockchain_block_header, double height,
        double cp_height) NOEXCEPT;
    void handle_blockchain_block_headers(const code& ec,
        rpc_interface::blockchain_block_headers, double start_height,
        double count, double cp_height) NOEXCEPT;
    void handle_blockchain_headers_subscribe(const code& ec,
        rpc_interface::blockchain_headers_subscribe) NOEXCEPT;
    void handle_blockchain_estimatefee(const code& ec,
        rpc_interface::blockchain_estimatefee, double) NOEXCEPT;
    void handle_blockchain_relayfee(const code& ec,
        rpc_interface::blockchain_relayfee) NOEXCEPT;
    void handle_blockchain_scripthash_get_balance(const code& ec,
        rpc_interface::blockchain_scripthash_get_balance,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_history(const code& ec,
        rpc_interface::blockchain_scripthash_get_history,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_mempool(const code& ec,
        rpc_interface::blockchain_scripthash_get_mempool,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_listunspent(const code& ec,
        rpc_interface::blockchain_scripthash_listunspent,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_subscribe(const code& ec,
        rpc_interface::blockchain_scripthash_subscribe,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_unsubscribe(const code& ec,
        rpc_interface::blockchain_scripthash_unsubscribe,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_transaction_broadcast(const code& ec,
        rpc_interface::blockchain_transaction_broadcast,
        const std::string& raw_tx) NOEXCEPT;
    void handle_blockchain_transaction_get(const code& ec,
        rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
        bool verbose) NOEXCEPT;
    void handle_blockchain_transaction_get_merkle(const code& ec,
        rpc_interface::blockchain_transaction_get_merkle,
        const std::string& tx_hash, double height) NOEXCEPT;
    void handle_blockchain_transaction_id_from_pos(const code& ec,
        rpc_interface::blockchain_transaction_id_from_pos, double height,
        double tx_pos, bool merkle) NOEXCEPT;

    /// Handlers (server).
    void handle_server_add_peer(const code& ec,
        rpc_interface::server_add_peer,
        const interface::object_t& features) NOEXCEPT;
    void handle_server_banner(const code& ec,
        rpc_interface::server_banner) NOEXCEPT;
    void handle_server_donation_address(const code& ec,
        rpc_interface::server_donation_address) NOEXCEPT;
    void handle_server_features(const code& ec,
        rpc_interface::server_features) NOEXCEPT;
    void handle_server_peers_subscribe(const code& ec,
        rpc_interface::server_peers_subscribe) NOEXCEPT;
    void handle_server_ping(const code& ec,
        rpc_interface::server_ping) NOEXCEPT;
    void handle_server_version(const code& ec,
        rpc_interface::server_version, const std::string& client_name,
        const interface::value_t& protocol_version) NOEXCEPT;

    /// Handlers (mempool).
    void handle_mempool_get_fee_histogram(const code& ec,
        rpc_interface::mempool_get_fee_histogram) NOEXCEPT;

protected:
    enum class protocol_version
    {
        /// Invalid version.
        v0_0,

        /// 2011, initial protocol negotiation.
        v0_6,

        /// 2012, enhanced protocol negotiation.
        v0_8,

        /// 2012, added pruning limits and transport indicators.
        v0_9,

        /// 2013, baseline for core methods in the official specification.
        v0_10,

        /// 2014, 1.x series, deprecations of utxo and block number methods.
        v1_0,

        /// 2015, updated version response and introduced scripthash methods.
        v1_1,

        /// 2017, added optional parameters for transactions and headers.
        v1_2,

        /// 2018, defaulted raw headers and introduced new block methods.
        v1_3,

        /// 2019, removed deserialized headers and added merkle proof features.
        v1_4,

        /// 2019, modifications for auxiliary proof-of-work handling.
        v1_4_1,

        /// 2020, added scripthash unsubscribe functionality.
        v1_4_2,

        /// 2022, updated response formats and added fee estimation modes.
        v1_6
    };

    static constexpr protocol_version minimum = protocol_version::v1_4;
    static constexpr protocol_version maximum = protocol_version::v1_4_2;

    protocol_version version() const NOEXCEPT;
    std::string_view get_version() const NOEXCEPT;
    bool is_version(protocol_version version) const NOEXCEPT;
    bool set_version(const interface::value_t& version) NOEXCEPT;
    bool get_versions(protocol_version& min, protocol_version& max,
        const interface::value_t& version) NOEXCEPT;

    static std::string_view get_server() NOEXCEPT;
    std::string_view get_client() const NOEXCEPT;
    std::string escape_client(const std::string& in) NOEXCEPT;
    bool set_client(const std::string& name) NOEXCEPT;

private:
    static std::string_view version_to_string(
        protocol_version version) NOEXCEPT;
    static protocol_version version_from_string(
        const std::string_view& version) NOEXCEPT;

    // These are protected by strand.
    protocol_version version_{ protocol_version::v0_0 };
    std::string name_{};
};

} // namespace node
} // namespace libbitcoin

#endif
