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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BITCOIND_RPC_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BITCOIND_RPC_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_http.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_bitcoind_rpc
  : public node::protocol_http,
    protected network::tracker<protocol_bitcoind_rpc>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_rpc> ptr;
    using rpc_interface = interface::bitcoind_rpc;
    using rpc_dispatcher = network::rpc::dispatcher<rpc_interface>;

    inline protocol_bitcoind_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_http(session, channel, options),
        network::tracker<protocol_bitcoind_rpc>(session->log)
    {
    }

    /// Public start is required.
    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using post = network::http::method::post;
    using options = network::http::method::options;

    /// Dispatch.
    void handle_receive_options(const code& ec,
        const network::http::method::options::cptr& options) NOEXCEPT override;
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;

    /// Handlers.
    bool handle_get_best_block_hash(const code& ec,
        rpc_interface::get_best_block_hash) NOEXCEPT;
    bool handle_get_block(const code& ec,
        rpc_interface::get_block, const std::string&, double) NOEXCEPT;
    bool handle_get_block_chain_info(const code& ec,
        rpc_interface::get_block_chain_info) NOEXCEPT;
    bool handle_get_block_count(const code& ec,
        rpc_interface::get_block_count) NOEXCEPT;
    bool handle_get_block_filter(const code& ec,
        rpc_interface::get_block_filter, const std::string&,
        const std::string&) NOEXCEPT;
    bool handle_get_block_hash(const code& ec,
        rpc_interface::get_block_hash, double) NOEXCEPT;
    bool handle_get_block_header(const code& ec,
        rpc_interface::get_block_header, const std::string&, bool) NOEXCEPT;
    bool handle_get_block_stats(const code& ec,
        rpc_interface::get_block_stats, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_get_chain_tx_stats(const code& ec,
        rpc_interface::get_chain_tx_stats, double, const std::string&) NOEXCEPT;
    bool handle_get_chain_work(const code& ec,
        rpc_interface::get_chain_work) NOEXCEPT;
    bool handle_get_tx_out(const code& ec,
        rpc_interface::get_tx_out, const std::string&, double, bool) NOEXCEPT;
    bool handle_get_tx_out_set_info(const code& ec,
        rpc_interface::get_tx_out_set_info) NOEXCEPT;
    bool handle_prune_block_chain(const code& ec,
        rpc_interface::prune_block_chain, double) NOEXCEPT;
    bool handle_save_mem_pool(const code& ec,
        rpc_interface::save_mem_pool) NOEXCEPT;
    bool handle_scan_tx_out_set(const code& ec,
        rpc_interface::scan_tx_out_set, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_verify_chain(const code& ec,
        rpc_interface::verify_chain, double, double) NOEXCEPT;
    bool handle_verify_tx_out_set(const code& ec,
        rpc_interface::verify_tx_out_set, const std::string&) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rpc_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // Send the response.
    void send_json(boost::json::value&& model, size_t size_hint) NOEXCEPT;

    // Provide the request for serialization, keeping it out of dispatch.
    void set_post(const post::cptr& post) NOEXCEPT;
    const post& get_post() const NOEXCEPT;

    // This is thread safe.
    ////const options_t& options_;

    // These are protected by strand.
    rpc_dispatcher rpc_dispatcher_{};
    post::cptr post_{};
};

} // namespace node
} // namespace libbitcoin

#endif
