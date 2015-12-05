/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOL_HEADER_SYNC_HPP
#define LIBBITCOIN_NODE_PROTOCOL_HEADER_SYNC_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace node {
        
/**
 * Headers sync protocol.
 */
class BCN_API protocol_header_sync
  : public network::protocol_timer, public track<protocol_header_sync>
{
public:
    typedef std::shared_ptr<protocol_header_sync> ptr;

    /**
     * Construct a header sync protocol instance.
     * @param[in]  pool          The thread pool used by the protocol.
     * @param[in]  channel       The channel on which to start the protocol.
     * @param[in]  minimum_rate  The minimum sync rate in headers per second.
     * @param[in]  first_height  The height of the first entry in headers.
     * @param[in]  hashes        The ordered set of block hashes when cmplete.
     * @param[in]  checkpoints   The ordered blockchain checkpoints.
     */
    protocol_header_sync(threadpool& pool, network::p2p&,
        network::channel::ptr channel, uint32_t minimum_rate,
        size_t first_height, hash_list& hashes,
        const config::checkpoint::list& checkpoints);

    /**
     * Start the protocol.
     * @param[in]  handler  The handler to call upon sync completion.
     */
    void start(event_handler handler);

private:
    static size_t target(size_t first_height, hash_list& headers,
        const config::checkpoint::list& checkpoints);

    size_t next_height() const;
    size_t current_rate() const;

    void rollback();
    bool merge_headers(const message::headers& message);
    bool checks(const hash_digest& hash, size_t height) const;
    bool chained(const chain::header& header, const hash_digest& hash) const;
    bool proof_of_work(const chain::header& header, size_t height) const;

    void send_get_headers(event_handler complete);
    void handle_send(const code& ec, event_handler complete);
    void handle_event(const code& ec, event_handler complete);
    void headers_complete(const code& ec, event_handler handler);
    bool handle_receive(const code& ec, const message::headers& message,
        event_handler complete);

    // This is write-guarded by the header message subscriber strand.
    hash_list& hashes_;

    // This is guarded by protocol_timer/deadline contract (exactly one call).
    size_t current_second_;

    const uint32_t minimum_rate_;
    const size_t start_size_;
    const size_t first_height_;
    const size_t target_height_;
    const config::checkpoint::list& checkpoints_;
};

} // namespace network
} // namespace libbitcoin

#endif
