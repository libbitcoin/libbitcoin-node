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
#include <bitcoin/node/rest/media.hpp>
#include <bitcoin/node/rest/parse.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_explore
    
using namespace system;
using namespace network::rpc;
using namespace network::http;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_explore::try_dispatch_object(const request& request) NOEXCEPT
{
    media_type accept{};
    if (!get_acceptable_media_type(accept, request))
    {
        send_bad_target(request);
        return true;
    }

    request_t out{};
    if (const auto ec = parse_request(out, request.target()))
    {
        send_bad_target(request, ec);
        return true;
    }

    ////const auto& query = archive();
    ////const auto wit = params["witness"] != "false";
    ////constexpr auto header_size = chain::header::serialized_size();

    ////if (accept == media_type::application_json)
    ////{
    ////    if (hd)
    ////    {
    ////        if (const auto ptr = query.get_header(query.to_header(hash)))
    ////        {
    ////            send_json(request, value_from(ptr), header_size);
    ////            return true;
    ////        }
    ////    }
    ////    else if (bk)
    ////    {
    ////        if (const auto ptr = query.get_block(query.to_header(hash), wit))
    ////        {
    ////            send_json(request, value_from(ptr), ptr->serialized_size(wit));
    ////            return true;
    ////        }
    ////    }
    ////    else
    ////    {
    ////        if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
    ////        {
    ////            send_json(request, value_from(ptr), ptr->serialized_size(wit));
    ////            return true;
    ////        }
    ////    }

    ////    send_not_found(request);
    ////    return true;
    ////}

    ////if (accept == media_type::text_plain)
    ////{
    ////    if (hd)
    ////    {
    ////        if (const auto ptr = query.get_header(query.to_header(hash)))
    ////        {
    ////            send_text(request, encode_base16(ptr->to_data()));
    ////            return true;
    ////        }
    ////    }
    ////    else if (bk)
    ////    {
    ////        if (const auto ptr = query.get_block(query.to_header(hash), wit))
    ////        {
    ////            send_text(request, encode_base16(ptr->to_data(wit)));
    ////            return true;
    ////        }
    ////    }
    ////    else
    ////    {
    ////        if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
    ////        {
    ////            send_text(request, encode_base16(ptr->to_data(wit)));
    ////            return true;
    ////        }
    ////    }

    ////    send_not_found(request);
    ////    return true;
    ////}

    ////////if (accept == media_type::application_octet_stream)
    ////{
    ////    if (hd)
    ////    {
    ////        if (const auto ptr = query.get_header(query.to_header(hash)))
    ////        {
    ////            send_data(request, ptr->to_data());
    ////            return true;
    ////        }
    ////    }
    ////    else if (bk)
    ////    {
    ////        if (const auto ptr = query.get_block(query.to_header(hash), wit))
    ////        {
    ////            send_data(request, ptr->to_data(wit));
    ////            return true;
    ////        }
    ////    }
    ////    else
    ////    {
    ////        if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
    ////        {
    ////            send_data(request, ptr->to_data(wit));
    ////            return true;
    ////        }
    ////    }

    ////    send_not_found(request);
    ////    return true;
    ////}
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
