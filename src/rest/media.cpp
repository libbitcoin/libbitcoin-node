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
#include <bitcoin/node/rest/media.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace network::http;

bool get_acceptable_media_type(media_type& out,
    const request& request) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(request.target()))
        return false;

    constexpr auto text = media_type::text_plain;
    constexpr auto json = media_type::application_json;
    constexpr auto data = media_type::application_octet_stream;
    const auto accepts = to_media_types((request)[field::accept]);
    const auto format = uri.decode_query()["format"];

    if (contains(accepts, json) || format == "json")
    {
        out = json;
        return true;
    }

    if (contains(accepts, text) || format == "text")
    {
        out = text;
        return true;
    }

    if (contains(accepts, data) || format == "data")
    {
        out = data;
        return true;
    }

    return false;
}

} // namespace node
} // namespace libbitcoin
