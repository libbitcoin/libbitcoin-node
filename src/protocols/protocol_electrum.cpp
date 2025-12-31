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
#include <bitcoin/node/protocols/protocol_electrum.hpp>

#include <algorithm>
#include <variant>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_electrum

using namespace interface;
using namespace std::placeholders;

constexpr auto max_client_name_length = 1024u;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_electrum::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Blockchain methods.
    SUBSCRIBE_RPC(handle_blockchain_block_header, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_RPC(handle_blockchain_headers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_estimatefee, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_relayfee, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_balance, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_history, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_mempool, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_listunspent, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_subscribe, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_unsubscribe, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_transaction_broadcast, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get_merkle, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_id_from_pos, _1, _2, _3, _4, _5);

    // Server methods
    SUBSCRIBE_RPC(handle_server_add_peer, _1, _2, _3);
    SUBSCRIBE_RPC(handle_server_banner, _1, _2);
    SUBSCRIBE_RPC(handle_server_donation_address, _1, _2);
    SUBSCRIBE_RPC(handle_server_features, _1, _2);
    SUBSCRIBE_RPC(handle_server_peers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_server_ping, _1, _2);
    SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);

    // Mempool methods.
    SUBSCRIBE_RPC(handle_mempool_get_fee_histogram, _1, _2);
    node::protocol_rpc<rpc_interface>::start();
}

// Handlers (blockchain).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_block_header(const code& ec,
    rpc_interface::blockchain_block_header, double ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_block_headers(const code& ec,
    rpc_interface::blockchain_block_headers, double ,
    double , double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_estimatefee(const code& ec,
    rpc_interface::blockchain_estimatefee, double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_relayfee(const code& ec,
    rpc_interface::blockchain_relayfee) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_balance(const code& ec,
    rpc_interface::blockchain_scripthash_get_balance,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_history(const code& ec,
    rpc_interface::blockchain_scripthash_get_history,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_mempool(const code& ec,
    rpc_interface::blockchain_scripthash_get_mempool,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_listunspent(const code& ec,
    rpc_interface::blockchain_scripthash_listunspent,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_unsubscribe(const code& ec,
    rpc_interface::blockchain_scripthash_unsubscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_broadcast(const code& ec,
    rpc_interface::blockchain_transaction_broadcast,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& ,
    bool ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_get_merkle(const code& ec,
    rpc_interface::blockchain_transaction_get_merkle, const std::string& ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_id_from_pos(const code& ec,
    rpc_interface::blockchain_transaction_id_from_pos, double ,
    double , bool ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

// Handlers (server).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_server_add_peer(const code& ec,
    rpc_interface::server_add_peer, const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_banner(const code& ec,
    rpc_interface::server_banner) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_donation_address(const code& ec,
    rpc_interface::server_donation_address) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_features(const code& ec,
    rpc_interface::server_features) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_peers_subscribe(const code& ec,
    rpc_interface::server_peers_subscribe) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_ping(const code& ec,
    rpc_interface::server_ping) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

// TODO: move to handshake protocol.
// TODO: strip extraneous args before dispatch.
// Changed in version 1.6: server must tolerate and ignore extraneous args.
void protocol_electrum::handle_server_version(const code& ec,
    rpc_interface::server_version, const std::string& client_name,
    const value_t& protocol_version) NOEXCEPT
{
    if (stopped(ec))
        return;

    // v0_0 implies version has not been set (first call).
    if ((version() == protocol_version::v0_0) &&
        (!set_client(client_name) || !set_version(protocol_version)))
    {
        send_code(error::invalid_argument);
        return;
    }

    send_result(value_t
        {
            array_t
            {
                { string_t{ get_server() } },
                { string_t{ get_version() } }
            }
        }, 70);
}

// Handlers (mempool).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_mempool_get_fee_histogram(const code& ec,
    rpc_interface::mempool_get_fee_histogram) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

// Client/server names.
// ----------------------------------------------------------------------------

// static
std::string_view protocol_electrum::get_server() NOEXCEPT
{
    return BC_USER_AGENT;
}

std::string_view protocol_electrum::get_client() const NOEXCEPT
{
    return name_;
}

bool protocol_electrum::set_client(const std::string& name) NOEXCEPT
{
    // Avoid excess, empty name is allowed.
    if (name.size() > max_client_name_length)
        return false;

    // Do not put to log without escaping.
    name_ = escape_client(name);
    return true;
}

std::string protocol_electrum::escape_client(const std::string& in) NOEXCEPT
{
    std::string out(in.size(), '*');
    std::transform(in.begin(), in.end(), out.begin(), [](char c) NOEXCEPT
    {
        using namespace system;
        return is_ascii_character(c) && !is_ascii_whitespace(c) ? c : '*';
    });

    return out;
}

// Negotiated version.
// ----------------------------------------------------------------------------

bool protocol_electrum::is_version(protocol_version version) const NOEXCEPT
{
    return version_ >= version;
}

protocol_electrum::protocol_version protocol_electrum::version() const NOEXCEPT
{
    return version_;
}

std::string_view protocol_electrum::get_version() const NOEXCEPT
{
    return version_to_string(version_);
}

bool protocol_electrum::set_version(const value_t& version) NOEXCEPT
{
    protocol_version client_min{};
    protocol_version client_max{};
    if (!get_versions(client_min, client_max, version))
        return false;

    const auto lower = std::max(client_min, minimum);
    const auto upper = std::min(client_max, maximum);
    if (lower > upper)
        return false;

    LOGA("Electrum [" << authority() << "] version ("
        << version_to_string(client_max) << ") " << get_client());

    version_ = upper;
    return true;
}

bool protocol_electrum::get_versions(protocol_version& min,
    protocol_version& max, const interface::value_t& version) NOEXCEPT
{
    // Optional value_t can be string_t or array_t of two string_t.
    const auto& value = version.value();

    // Default version (null_t is the default of value_t).
    if (std::holds_alternative<null_t>(value))
    {
        // An interface default can't be set for optional<value_t>.
        max = min = protocol_version::v1_4;
        return true;
    }

    // One version.
    if (std::holds_alternative<string_t>(value))
    {
        // A single value implies minimum is the same as maximum.
        max = min = version_from_string(std::get<string_t>(value));
        return min != protocol_version::v0_0;
    }

    // Two versions.
    if (std::holds_alternative<array_t>(value))
    {
        const auto& versions = std::get<array_t>(value);
        if (versions.size() != two)
            return false;

        // First string is mimimum, second is maximum.
        const auto& min_version = versions.at(0).value();
        const auto& max_version = versions.at(1).value();
        if (!std::holds_alternative<string_t>(min_version) ||
            !std::holds_alternative<string_t>(max_version))
            return false;

        min = version_from_string(std::get<string_t>(min_version));
        max = version_from_string(std::get<string_t>(max_version));
        return min != protocol_version::v0_0
            && max != protocol_version::v0_0;
    }

    return false;
}

// private/static
std::string_view protocol_electrum::version_to_string(
    protocol_version version) NOEXCEPT
{
    static const std::unordered_map<protocol_version, std::string_view> map
    {
        { protocol_version::v0_6,   "0.6" },
        { protocol_version::v0_8,   "0.8" },
        { protocol_version::v0_9,   "0.9" },
        { protocol_version::v0_10,  "0.10" },
        { protocol_version::v1_0,   "1.0" },
        { protocol_version::v1_1,   "1.1" },
        { protocol_version::v1_2,   "1.2" },
        { protocol_version::v1_3,   "1.3" },
        { protocol_version::v1_4,   "1.4" },
        { protocol_version::v1_4_1, "1.4.1" },
        { protocol_version::v1_4_2, "1.4.2" },
        { protocol_version::v1_6,   "1.6" },
        { protocol_version::v0_0,   "0.0" }
    };

    const auto it = map.find(version);
    return it != map.end() ? it->second : "0.0";
}

// private/static
protocol_electrum::protocol_version protocol_electrum::version_from_string(
    const std::string_view& version) NOEXCEPT
{
    static const std::unordered_map<std::string_view, protocol_version> map
    {
        { "0.6",   protocol_version::v0_6 },
        { "0.8",   protocol_version::v0_8 },
        { "0.9",   protocol_version::v0_9 },
        { "0.10",  protocol_version::v0_10 },
        { "1.0",   protocol_version::v1_0 },
        { "1.1",   protocol_version::v1_1 },
        { "1.2",   protocol_version::v1_2 },
        { "1.3",   protocol_version::v1_3 },
        { "1.4",   protocol_version::v1_4 },
        { "1.4.1", protocol_version::v1_4_1 },
        { "1.4.2", protocol_version::v1_4_2 },
        { "1.6",   protocol_version::v1_6 },
        { "0.0",   protocol_version::v0_0 }
    };

    const auto it = map.find(version);
    return it != map.end() ? it->second : protocol_version::v0_0;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
