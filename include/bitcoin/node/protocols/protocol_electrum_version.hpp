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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP

#include <memory>
#include <unordered_map>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_electrum_version
  : public node::protocol_rpc<channel_electrum>,
    protected network::tracker<protocol_electrum_version>
{
public:
    typedef std::shared_ptr<protocol_electrum_version> ptr;
    using rpc_interface = interface::electrum;

    inline protocol_electrum_version(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_rpc<channel_electrum>(session, channel, options),
        network::tracker<protocol_electrum_version>(session->log)
    {
    }

    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    virtual void complete(const code& ec, const code& shake) NOEXCEPT;

protected:
    void handle_server_version(const code& ec,
        rpc_interface::server_version, const std::string& client_name,
        const interface::value_t& protocol_version) NOEXCEPT;

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
    bool set_version(const interface::value_t& version) NOEXCEPT;
    bool get_versions(protocol_version& min, protocol_version& max,
        const interface::value_t& version) NOEXCEPT;

    std::string_view get_server() const NOEXCEPT;
    std::string_view get_client() const NOEXCEPT;
    std::string escape_client(const std::string& in) NOEXCEPT;
    bool set_client(const std::string& name) NOEXCEPT;

private:
    static std::string_view version_to_string(
        protocol_version version) NOEXCEPT;
    static protocol_version version_from_string(
        const std::string_view& version) NOEXCEPT;

    // These are protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
    protocol_version version_{ protocol_version::v0_0 };
    std::string name_{};
};

} // namespace node
} // namespace libbitcoin

#endif
