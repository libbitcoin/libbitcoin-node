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
#include <bitcoin/node/settings.hpp>

#include <bitcoin/network.hpp>

using namespace bc::system;

namespace libbitcoin {
namespace log {

settings::settings() NOEXCEPT
  : verbose(false),
    maximum_size(1'000'000'000_u32),
    file("rotate")
{
}

settings::settings(chain::selection) NOEXCEPT
  : settings()
{
}

std::filesystem::path settings::file1() NOEXCEPT
{
    std::filesystem::path out{ file };
    out.concat("1.log");
    return out;
}

std::filesystem::path settings::file2() NOEXCEPT
{
    std::filesystem::path out{ file };
    out.concat("2.log");
    return out;
}

} // namespace log

namespace node {

settings::settings() NOEXCEPT
{
}

settings::settings(chain::selection) NOEXCEPT
  : settings()
{
}

} // namespace node
} // namespace libbitcoin
