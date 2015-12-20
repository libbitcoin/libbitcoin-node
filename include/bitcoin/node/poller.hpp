/**
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
#ifndef LIBBITCOIN_NODE_POLLER_HPP
#define LIBBITCOIN_NODE_POLLER_HPP

#include <atomic>
#include <cstddef>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API poller
{
public:
    poller(chain::blockchain& chain, size_t minimum_start_height);
    void monitor(bc::network::channel_ptr node);
    void request_blocks(const hash_digest& block_hash,
        bc::network::channel_ptr node);

private:
    bool handle_reorg(const std::error_code& ec, uint32_t fork_point,
        const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list&);

    bool receive_block(const std::error_code& ec,
        const block_type& block, bc::network::channel_ptr node);
    void handle_store_block(const std::error_code& ec, block_info info,
        const hash_digest& block_hash, bc::network::channel_ptr node);

    void handle_poll(const std::error_code& ec, network::channel_ptr node);
    void ask_blocks(const std::error_code& ec,
        const block_locator_type& locator, const hash_digest& hash_stop,
        bc::network::channel_ptr node);

    chain::blockchain& blockchain_;
    std::atomic<uint64_t> last_height_;
    std::atomic<hash_digest> last_block_;
    const size_t minimum_start_height_;
};

} // namespace node
} // namespace libbitcoin

#endif

