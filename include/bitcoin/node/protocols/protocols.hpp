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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOLS_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOLS_HPP

/// base
#include <bitcoin/node/protocols/protocol.hpp>
#include <bitcoin/node/protocols/protocol_html.hpp>
#include <bitcoin/node/protocols/protocol_http.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

/// peer
#include <bitcoin/node/protocols/protocol_block_in_106.hpp>
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>
#include <bitcoin/node/protocols/protocol_block_out_106.hpp>
#include <bitcoin/node/protocols/protocol_block_out_70012.hpp>
#include <bitcoin/node/protocols/protocol_filter_out_70015.hpp>
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>
#include <bitcoin/node/protocols/protocol_header_in_70012.hpp>
#include <bitcoin/node/protocols/protocol_header_out_31800.hpp>
#include <bitcoin/node/protocols/protocol_header_out_70012.hpp>
#include <bitcoin/node/protocols/protocol_observer.hpp>
#include <bitcoin/node/protocols/protocol_performer.hpp>
#include <bitcoin/node/protocols/protocol_transaction_in_106.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out_106.hpp>

/// server
#include <bitcoin/node/protocols/protocol_web.hpp>
#include <bitcoin/node/protocols/protocol_explore.hpp>
#include <bitcoin/node/protocols/protocol_bitcoind_rest.hpp>
#include <bitcoin/node/protocols/protocol_bitcoind_rpc.hpp>
#include <bitcoin/node/protocols/protocol_electrum.hpp>
#include <bitcoin/node/protocols/protocol_stratum_v1.hpp>
#include <bitcoin/node/protocols/protocol_stratum_v2.hpp>

#endif
