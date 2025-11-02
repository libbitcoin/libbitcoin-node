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
#include <bitcoin/node/protocols/protocol_explore.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_explore

using namespace boost::json;
using namespace std::placeholders;
using namespace network::http;
using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_explore::try_dispatch_object(const request& request) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(request.target()))
    {
        send_bad_target(request);
        return true;
    }

    const auto parts = split(uri.path(), "/");
    if (parts.size() != two)
        return false;

    const auto hd = parts.front() == "header"      || parts.front() == "hd";
    const auto bk = parts.front() == "block"       || parts.front() == "bk";
    const auto tx = parts.front() == "transaction" || parts.front() == "tx";
    if (!hd && !bk && !tx)
    {
        send_bad_target(request);
        return true;
    }

    auto params = uri.decode_query();
    const auto format = params["format"];
    constexpr auto text = mime_type::text_plain;
    constexpr auto json = mime_type::application_json;
    constexpr auto data = mime_type::application_octet_stream;
    const auto accepts = to_mime_types((request)[field::accept]);
    const auto is_json = contains(accepts, json) || format == "json";
    const auto is_text = contains(accepts, text) || format == "text";
    const auto is_data = contains(accepts, data) || format == "data";
    if (!is_json && !is_text && !is_data)
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, parts.back()))
    {
        send_bad_target(request);
        return true;
    }

    const auto& query = archive();
    const auto wit = params["witness"] != "false";

    if (is_json)
    {
        if (hd)
        {
            if (const auto ptr = query.get_header(query.to_header(hash)))
            {
                send_json(request, value_from(ptr), ptr->serialized_size());
                return true;
            }
        }
        else if (bk)
        {
            if (const auto ptr = query.get_block(query.to_header(hash), wit))
            {
                send_json(request, value_from(ptr), ptr->serialized_size(wit));
                return true;
            }
        }
        else
        {
            if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
            {
                send_json(request, value_from(ptr), ptr->serialized_size(wit));
                return true;
            }
        }

        send_not_found(request);
        return true;
    }

    if (is_text)
    {
        if (hd)
        {
            if (const auto ptr = query.get_header(query.to_header(hash)))
            {
                send_text(request, encode_base16(ptr->to_data()));
                return true;
            }
        }
        else if (bk)
        {
            if (const auto ptr = query.get_block(query.to_header(hash), wit))
            {
                send_text(request, encode_base16(ptr->to_data(wit)));
                return true;
            }
        }
        else
        {
            if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
            {
                send_text(request, encode_base16(ptr->to_data(wit)));
                return true;
            }
        }

        send_not_found(request);
        return true;
    }

    ////if (is_data)
    {
        if (hd)
        {
            if (const auto ptr = query.get_header(query.to_header(hash)))
            {
                send_data(request, ptr->to_data());
                return true;
            }
        }
        else if (bk)
        {
            if (const auto ptr = query.get_block(query.to_header(hash), wit))
            {
                send_data(request, ptr->to_data(wit));
                return true;
            }
        }
        else
        {
            if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
            {
                send_data(request, ptr->to_data(wit));
                return true;
            }
        }

        send_not_found(request);
        return true;
    }
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
