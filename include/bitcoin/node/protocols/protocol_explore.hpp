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

    protocol_explore(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_html(session, channel, options),
        network::tracker<protocol_explore>(session->log)
    {
    }

    /// Public start is required.
    void start() NOEXCEPT override
    {
        node::protocol_html::start();
    }

protected:

    /// TODO: move to own source.
    /// Pagination and filtering are via query string.
    enum targets
    {
        /// /v[]/block/hash/[bkhash] {1}
        /// /v[]/block/height/[height] {1}s
        block,

        /// /v[]/block/hash/[bkhash]/filter {1}
        /// /v[]/block/height/[height]/filter {1}
        filter,

        /// /v[]/block/hash/[bkhash]/header {1}
        /// /v[]/block/height/[height]/header {1}
        header,

        /// /v[]/transaction/hash/[txhash] {1}
        /// /v[]/block/hash/[bkhash]/transaction/[position] {1}
        /// /v[]/block/height/[height]/transaction/[position] {1}
        transaction,

        /// /v[]/block/hash/[bkhash]/transactions {all txs in the block}
        /// /v[]/block/height/[height]/transactions {all txs in the block}
        transactions,

        // --------------------------------------------------------------------

        /// /v[]/input/[txhash]/[index] {1}
        input,

        /// /v[]/inputs/[txhash] {all inputs in the tx}
        inputs,

        /// /v[]/input/[txhash]/[index]/script {1}
        input_script,

        /// /v[]/input/[txhash]/scripts {all input scripts in the tx}
        input_scripts,

        /// /v[]/input/[txhash]/[index]/witness {1}
        input_witness,

        /// /v[]/input/[txhash]/witnesses {all witnesses in the tx}
        input_witnesses,

        // --------------------------------------------------------------------

        /// /v[]/output/[txhash]/[index] {1}
        output,

        /// /v[]/outputs/[txhash] {all outputs in the tx}
        outputs,

        /// /v[]/output/[txhash]/[index]/script {1}
        output_script,

        /// /v[]/output/[txhash]/scripts {all output scripts in the tx}
        output_scripts,

        /// /v[]/output/[txhash]/[index]/spender {1 - confirmed}
        output_spender,

        /// /v[]/output/[txhash]/spenders {all}
        output_spenders,

        // --------------------------------------------------------------------

        /// /v[]/address/[output-script-hash] {all}
        address
    };

    /// Senders.
    void send_json(const network::http::request& request,
        boost::json::value&& model) NOEXCEPT;
    void send_data(const network::http::request& request,
        system::data_chunk&& data) NOEXCEPT;
    void send_text(const network::http::request& request,
        std::string&& hexidecimal) NOEXCEPT;

    /// Receivers.
    void handle_receive_get(const code& ec,
        const network::http::method::get& request) NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#endif
