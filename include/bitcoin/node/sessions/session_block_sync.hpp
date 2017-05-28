/////**
//// * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
//// *
//// * This file is part of libbitcoin.
//// *
//// * This program is free software: you can redistribute it and/or modify
//// * it under the terms of the GNU Affero General Public License as published by
//// * the Free Software Foundation, either version 3 of the License, or
//// * (at your option) any later version.
//// *
//// * This program is distributed in the hope that it will be useful,
//// * but WITHOUT ANY WARRANTY; without even the implied warranty of
//// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// * GNU Affero General Public License for more details.
//// *
//// * You should have received a copy of the GNU Affero General Public License
//// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//// */
////#ifndef LIBBITCOIN_NODE_SESSION_BLOCK_SYNC_HPP
////#define LIBBITCOIN_NODE_SESSION_BLOCK_SYNC_HPP
////
////#include <cstddef>
////#include <cstdint>
////#include <memory>
////#include <bitcoin/blockchain.hpp>
////#include <bitcoin/network.hpp>
////#include <bitcoin/node/define.hpp>
////#include <bitcoin/node/sessions/session.hpp>
////#include <bitcoin/node/settings.hpp>
////#include <bitcoin/node/utility/check_list.hpp>
////#include <bitcoin/node/utility/reservation.hpp>
////#include <bitcoin/node/utility/reservations.hpp>
////
////namespace libbitcoin {
////namespace node {
////
////class full_node;
////
/////// Class to manage initial block download connections, thread safe.
////class BCN_API session_block_sync
////  : public session<network::session_outbound>, track<session_block_sync>
////{
////public:
////    typedef std::shared_ptr<session_block_sync> ptr;
////
////    session_block_sync(full_node& network, check_list& hashes,
////        blockchain::fast_chain& chain, const settings& settings);
////
////    void start(result_handler handler) override;
////
////protected:
////    /// Overridden to attach and start specialized handshake.
////    void attach_handshake_protocols(network::channel::ptr channel,
////        result_handler handle_started) override;
////
////    /// Override to attach and start specialized protocols after handshake.
////    virtual void attach_protocols(network::channel::ptr channel,
////        reservation::ptr row, result_handler handler);
////
////private:
////    void handle_started(const code& ec, result_handler handler);
////    void new_connection(reservation::ptr row, result_handler handler);
////
////    // Sequence.
////    void handle_connect(const code& ec, network::channel::ptr channel,
////        reservation::ptr row, result_handler handler);
////    void handle_channel_start(const code& ec, network::channel::ptr channel,
////        reservation::ptr row, result_handler handler);
////    void handle_channel_complete(const code& ec, reservation::ptr row,
////        result_handler handler);
////    void handle_channel_stop(const code& ec, reservation::ptr row);
////    void handle_complete(const code& ec, result_handler handler);
////
////    // Timers.
////    void reset_timer();
////    void handle_timer(const code& ec);
////
////    // These are thread safe.
////    blockchain::fast_chain& chain_;
////    reservations reservations_;
////    deadline::ptr timer_;
////};
////
////} // namespace node
////} // namespace libbitcoin
////
////#endif
////
