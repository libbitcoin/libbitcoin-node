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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_EXPLORE_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_EXPLORE_HPP

#include <memory>
#include <optional>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_html.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_explore
  : public node::protocol_html,
    protected network::tracker<protocol_explore>
{
public:
    typedef std::shared_ptr<protocol_explore> ptr;
    using interface = network::rpc::interface::explore;
    using dispatcher = network::rpc::dispatcher<interface>;

    protocol_explore(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_html(session, channel, options),
        network::tracker<protocol_explore>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Dispatch.
    bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT override;

    /// REST interface handlers.

    bool handle_get_block(const code& ec, interface::block,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height, bool witness) NOEXCEPT;
    bool handle_get_header(const code& ec, interface::header,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_txs(const code& ec, interface::block_txs,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_tx(const code& ec, interface::block_tx,
        uint8_t version, uint8_t media, uint32_t position,
        std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height, bool witness) NOEXCEPT;
    bool handle_get_transaction(const code& ec, interface::transaction,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        bool witness) NOEXCEPT;
    bool handle_get_tx_block(const code& ec, interface::tx_block,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;

    bool handle_get_inputs(const code& ec, interface::inputs,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        bool witness) NOEXCEPT;
    bool handle_get_input(const code& ec, interface::input,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index, bool witness) NOEXCEPT;
    bool handle_get_input_script(const code& ec, interface::input_script,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_input_witness(const code& ec, interface::input_witness,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;

    bool handle_get_outputs(const code& ec, interface::outputs,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_output(const code& ec, interface::output,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_script(const code& ec, interface::output_script,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_spender(const code& ec, interface::output_spender,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_spenders(const code& ec, interface::output_spender,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;

    bool handle_get_address(const code& ec, interface::address,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_filter(const code& ec, interface::filter, uint8_t version,
        uint8_t media, uint8_t type, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_filter_hash(const code& ec, interface::filter_hash,
        uint8_t version, uint8_t media, uint8_t type,
        std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_filter_header(const code& ec, interface::filter_header,
        uint8_t version, uint8_t media, uint8_t type,
        std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;

private:
    database::header_link to_header(const std::optional<uint32_t>& height,
        const std::optional<system::hash_cptr>& hash) NOEXCEPT;

    dispatcher dispatcher_{};
};

} // namespace node
} // namespace libbitcoin

#endif
