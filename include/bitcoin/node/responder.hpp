/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_RESPONDER_HPP
#define LIBBITCOIN_NODE_RESPONDER_HPP

#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API responder
{
public:
    responder(blockchain::blockchain& chain,
        blockchain::transaction_pool& tx_pool);

    void monitor(bc::network::channel::pointer node);

private:
    void receive_get_data(const std::error_code& ec,
        const message::get_data& packet, bc::network::channel::pointer node);

    void send_pool_tx(const std::error_code& ec, const chain::transaction& tx,
        const hash_digest& tx_hash, bc::network::channel::pointer node);
    void send_chain_tx(const std::error_code& ec, const chain::transaction& tx,
        const hash_digest& tx_hash, bc::network::channel::pointer node);
    void send_tx(const chain::transaction& tx, const hash_digest& tx_hash,
        bc::network::channel::pointer node);
    void send_tx_not_found(const hash_digest& tx_hash,
        bc::network::channel::pointer node);

    void send_block(const std::error_code& ec, const chain::block& block,
        const hash_digest& block_hash, bc::network::channel::pointer node);
    void send_block_not_found(const hash_digest& block_hash,
        bc::network::channel::pointer node);

    void send_inventory_not_found(message::inventory_type_id inventory_type,
        const hash_digest& hash, bc::network::channel::pointer node,
        bc::network::channel_proxy::send_handler handler);

    blockchain::blockchain& blockchain_;
    blockchain::transaction_pool& tx_pool_;
};

} // node
} // libbitcoin

#endif
