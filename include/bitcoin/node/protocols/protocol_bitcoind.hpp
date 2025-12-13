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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BITCOIND_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BITCOIND_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_http.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_bitcoind
  : public node::protocol_http,
    protected network::tracker<protocol_bitcoind>
{
public:
    typedef std::shared_ptr<protocol_bitcoind> ptr;
    using interface = interface::bitcoind;
    using dispatcher = network::rpc::dispatcher<interface>;

    inline protocol_bitcoind(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_http(session, channel, options),
        network::tracker<protocol_bitcoind>(session->log)
    {
    }

    /// Public start is required.
    void start() NOEXCEPT override;

protected:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Dispatch.
    void handle_receive_post(const code& ec,
        const network::http::method::post::cptr& post) NOEXCEPT override;

    /// Handlers.
    bool handle_get_best_block_hash(const code& ec,
        interface::get_best_block_hash) NOEXCEPT;
    bool handle_get_block(const code& ec,
        interface::get_block, const std::string&, double) NOEXCEPT;
    bool handle_get_block_chain_info(const code& ec,
        interface::get_block_chain_info) NOEXCEPT;
    bool handle_get_block_count(const code& ec,
        interface::get_block_count) NOEXCEPT;
    bool handle_get_block_filter(const code& ec,
        interface::get_block_filter, const std::string&,
        const std::string&) NOEXCEPT;
    bool handle_get_block_hash(const code& ec,
        interface::get_block_hash, double) NOEXCEPT;
    bool handle_get_block_header(const code& ec,
        interface::get_block_header, const std::string&, bool) NOEXCEPT;
    bool handle_get_block_stats(const code& ec,
        interface::get_block_stats, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_get_chain_tx_stats(const code& ec,
        interface::get_chain_tx_stats, double, const std::string&) NOEXCEPT;
    bool handle_get_chain_work(const code& ec,
        interface::get_chain_work) NOEXCEPT;
    bool handle_get_tx_out(const code& ec,
        interface::get_tx_out, const std::string&, double, bool) NOEXCEPT;
    bool handle_get_tx_out_set_info(const code& ec,
        interface::get_tx_out_set_info) NOEXCEPT;
    bool handle_prune_block_chain(const code& ec,
        interface::prune_block_chain, double) NOEXCEPT;
    bool handle_save_mem_pool(const code& ec,
        interface::save_mem_pool) NOEXCEPT;
    bool handle_scan_tx_out_set(const code& ec,
        interface::scan_tx_out_set, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_verify_chain(const code& ec,
        interface::verify_chain, double, double) NOEXCEPT;
    bool handle_verify_tx_out_set(const code& ec,
        interface::verify_tx_out_set, const std::string&) NOEXCEPT;

private:
    // This is thread safe.
    ////const options_t& options_;

    // This is protected by strand.
    dispatcher dispatcher_{};
};

} // namespace node
} // namespace libbitcoin

#endif
