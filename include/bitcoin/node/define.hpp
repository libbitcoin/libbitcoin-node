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
#ifndef LIBBITCOIN_NODE_DEFINE_HPP
#define LIBBITCOIN_NODE_DEFINE_HPP

#include <bitcoin/bitcoin.hpp>

// We use the generic helper definitions in libbitcoin to define BCN_API 
// and BCN_INTERNAL. BCN_API is used for the public API symbols. It either DLL
// imports or DLL exports (or does nothing for static build) BCN_INTERNAL is 
// used for non-api symbols.

#if defined BCN_STATIC
    #define BCN_API
    #define BCN_INTERNAL
#elif defined BCN_DLL
    #define BCN_API      BC_HELPER_DLL_EXPORT
    #define BCN_INTERNAL BC_HELPER_DLL_LOCAL
#else
    #define BCN_API      BC_HELPER_DLL_IMPORT
    #define BCN_INTERNAL BC_HELPER_DLL_LOCAL
#endif

// Log name.
#define LOG_NODE "node"

namespace libbitcoin {
namespace node {

typedef message::fee_filter::ptr fee_filter_ptr;
typedef message::fee_filter::const_ptr fee_filter_const_ptr;

typedef message::get_blocks::ptr get_blocks_ptr;
typedef message::get_blocks::const_ptr get_blocks_const_ptr;

typedef message::get_data::ptr get_data_ptr;
typedef message::get_data::const_ptr get_data_const_ptr;

typedef message::get_headers::ptr get_headers_ptr;
typedef message::get_headers::const_ptr get_headers_const_ptr;

typedef message::header_message::ptr header_ptr;
typedef message::header_message::const_ptr header_const_ptr;

typedef message::headers::ptr headers_ptr;
typedef message::headers::const_ptr headers_const_ptr;

typedef message::inventory::ptr inventory_ptr;
typedef message::inventory::const_ptr inventory_const_ptr;

typedef message::memory_pool::ptr memory_pool_ptr;
typedef message::memory_pool::const_ptr memory_pool_const_ptr;

typedef message::not_found::ptr not_found_ptr;
typedef message::not_found::const_ptr not_found_const_ptr;

typedef message::merkle_block::ptr merkle_block_ptr;
typedef message::merkle_block::const_ptr merkle_block_const_ptr;

typedef message::not_found::ptr not_found_ptr;
typedef message::not_found::const_ptr not_found_ptr_const_ptr;

typedef message::send_headers::ptr send_headers_ptr;
typedef message::send_headers::const_ptr send_headers_const_ptr;

typedef message::block_message::ptr block_ptr;
typedef message::block_message::const_ptr block_const_ptr;
typedef message::block_message::ptr_list block_ptr_list;
typedef message::block_message::const_ptr_list block_const_ptr_list;

typedef message::transaction_message::ptr transaction_ptr;
typedef message::transaction_message::const_ptr transaction_const_ptr;
typedef message::transaction_message::ptr_list transaction_ptr_list;
typedef message::transaction_message::const_ptr_list
    transaction_const_ptr_list;

} // namespace node
} // namespace libbitcoin

#endif
