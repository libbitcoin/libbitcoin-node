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
#ifndef LIBBITCOIN_NODE_DISPATCH_HPP
#define LIBBITCOIN_NODE_DISPATCH_HPP

#include <iostream>
#include <bitcoin/node.hpp>

enum console_result : int
{
    failure = -1,
    okay = 0,
    not_started = 1
};

namespace libbitcoin {
namespace node {

console_result run(p2p_node& node);
void monitor_stop(p2p_node& node, p2p_node::result_handler);

void handle_started(const code& ec, p2p_node& node);
void handle_running(const code& ec);

// Load arguments and config and then run the node.
console_result dispatch(int argc, const char* argv[], std::istream&,
    std::ostream& output, std::ostream& error);

} // namespace node
} // namespace libbitcoin

#endif