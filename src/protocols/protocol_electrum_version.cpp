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
#include <bitcoin/node/protocols/protocol_electrum_version.hpp>

#include <algorithm>
#include <variant>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_electrum_version

using namespace network;
using namespace interface;
using namespace std::placeholders;

constexpr auto max_client_name_length = 1024u;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/complete (handshake).
// ----------------------------------------------------------------------------

// Session resumes the channel following return from start().
// Sends are not precluded, but no messages can be received while paused.
void protocol_electrum_version::shake(result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    handler_ = system::move_shared<result_handler>(std::move(handler));

    SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);
    protocol_rpc<channel_electrum>::start();
}

void protocol_electrum_version::complete(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (handler_)
    {
        // shake error will result in stopped channel.
        (*handler_)(shake);
        handler_.reset();
    }
}

// Handler.
// ----------------------------------------------------------------------------

// Changed in version 1.6: server must tolerate and ignore extraneous args.
void protocol_electrum_version::handle_server_version(const code& ec,
    rpc_interface::server_version, const std::string& client_name,
    const value_t& protocol_version) NOEXCEPT
{
    if (stopped(ec))
        return;

    // v0_0 implies version has not been set (first call).
    if ((version() == protocol_version::v0_0) &&
        (!set_client(client_name) || !set_version(protocol_version)))
    {
        const auto reason = error::invalid_argument;
        send_code(reason, BIND(complete, _1, reason));
    }
    else
    {
        send_result(value_t
            {
                array_t
                {
                    { string_t{ get_server() } },
                    { string_t{ get_version() } }
                }
            }, 70, BIND(complete, _1, error::success));
    }
}

// Client/server names.
// ----------------------------------------------------------------------------

// static
std::string_view protocol_electrum_version::get_server() NOEXCEPT
{
    return BC_USER_AGENT;
}

std::string_view protocol_electrum_version::get_client() const NOEXCEPT
{
    return name_;
}

bool protocol_electrum_version::set_client(const std::string& name) NOEXCEPT
{
    // Avoid excess, empty name is allowed.
    if (name.size() > max_client_name_length)
        return false;

    // Do not put to log without escaping.
    name_ = escape_client(name);
    return true;
}

std::string protocol_electrum_version::escape_client(const std::string& in) NOEXCEPT
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

protocol_electrum_version::protocol_version
protocol_electrum_version::version() const NOEXCEPT
{
    return version_;
}

std::string_view protocol_electrum_version::get_version() const NOEXCEPT
{
    return version_to_string(version_);
}

bool protocol_electrum_version::set_version(const value_t& version) NOEXCEPT
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

bool protocol_electrum_version::get_versions(protocol_version& min,
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
std::string_view
protocol_electrum_version::version_to_string(protocol_version version) NOEXCEPT
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
protocol_electrum_version::protocol_version 
protocol_electrum_version::version_from_string(
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
