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
#include "stack_trace.hpp"

#ifdef HAVE_MSC

namespace libbitcoin
{
namespace system
{
    std::istream& cin = cin_stream();
    std::ostream& cout = cout_stream();
    std::ostream& cerr = cerr_stream();
    int main(int argc, char* argv[]);
}
}

int wmain(int argc, wchar_t* argv[])
{
    __try
    {
        return libbitcoin::system::call_utf8_main(argc, argv,
            &libbitcoin::system::main);
    }
    __except (dump_stack_trace(GetExceptionInformation()))
    {
        return 1;
    }
}

// This is invoked by dump_stack_trace.
void handle_stack_trace(const std::string& trace) NOEXCEPT
{
    libbitcoin::system::cout << trace;
    libbitcoin::system::cout.flush();
}

// This is invoked by dump_stack_trace.
std::string pdb_path() NOEXCEPT
{
    return {};
}

#else
BC_USE_LIBBITCOIN_MAIN
#endif

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
