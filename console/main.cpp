/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <iostream>
#include <bitcoin/node.hpp>
#include "executor.hpp"

BC_USE_LIBBITCOIN_MAIN

/// Invoke this program with the raw arguments provided on the command line.
/// All console input and output streams for the application originate here.
int bc::system::main(int argc, char* argv[])
{
    using namespace bc;
    using namespace bc::node;
    using namespace bc::system;

    set_utf8_stdio();
    parser metadata(chain::selection::mainnet);
    const auto& args = const_cast<const char**>(argv);

    if (!metadata.parse(argc, args, cerr))
        return -1;

    executor host(metadata, cin, cout, cerr);
    return host.menu() ? 0 : -1;
}
