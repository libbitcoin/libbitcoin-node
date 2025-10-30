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

const auto binary = mime_type::application_octet_stream;
const auto json = mime_type::application_json;
const auto text = mime_type::text_plain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Handle get method.
// ----------------------------------------------------------------------------

void protocol_explore::handle_receive_get(const code& ec,
    const method::get& request) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");

    if (stopped(ec))
        return;

    // Enforce http origin policy (requires configured hosts).
    if (!is_allowed_origin(*request, request->version()))
    {
        send_forbidden(*request);
        return;
    }

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*request, request->version()))
    {
        send_bad_host(*request);
        return;
    }

    const auto target = request->target();
    if (!is_origin_form(target))
    {
        send_bad_target(*request);
        return;
    }

    wallet::uri uri{};
    if (!uri.decode(target))
    {
        send_bad_target(*request);
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: json error responses.
    // TODO: priority sort and dispatch.
    // TODO: trivial parse, the path is the base16 hash.
    ///////////////////////////////////////////////////////////////////////////

    const auto get_tx = [&](const auto& path) NOEXCEPT
    { 
        hash_digest hash{};
        if (!decode_hash(hash, path))
        {
            send_bad_target(*request);
            return chain::transaction::cptr{};
        }

        const auto& query = archive();
        return query.get_transaction(query.to_tx(hash), true);
    };

    auto params = uri.decode_query();
    const auto hex = trim_copy(uri.path(), { "/" });
    const auto accepts = to_mime_types((*request)[field::accept]);

    if (contains(accepts, json) || params["format"] == "json")
    {
        const auto tx = get_tx(hex);
        if (!tx)
        {
            send_not_found(*request);
            return;
        }

        send_json(*request, value_from(*tx));
        return;
    }

    if (contains(accepts, binary) || params["format"] == "binary")
    {
        const auto tx = get_tx(hex);
        if (!tx)
        {
            send_not_found(*request);
            return;
        }

        send_data(*request, tx->to_data(true));
        return;
    }

    if (contains(accepts, text) || params["format"] == "text")
    {
        const auto tx = get_tx(hex);
        if (!tx)
        {
            send_not_found(*request);
            return;
        }

        send_text(*request, encode_base16(tx->to_data(true)));
        return;
    }

    // Default to html (single page site).

    // Empty path implies malformed target (terminal).
    auto path = to_local_path(request->target());
    if (path.empty())
    {
        send_bad_target(*request);
        return;
    }

    if (!path.has_extension())
    {
        // Empty path implies default page is invalid or not configured.
        path = to_local_path();
        if (path.empty())
        {
            send_not_implemented(*request);
            return;
        }
    }

    // Not open implies file not found (non-terminal).
    auto file = get_file_body(path);
    if (!file.is_open())
    {
        send_not_found(*request);
        return;
    }

    send_file(*request, std::move(file), file_mime_type(path));
}

// TODO: buffer should be reused, so set at the channel.
// json_value is not a sized body, so this sets chunked encoding.
void protocol_explore::send_json(const request& request,
    boost::json::value&& model) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(json));
    response.body() = { std::move(model) };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_explore::send_data(const request& request,
    data_chunk&& data) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    data_response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(binary));
    response.body() = std::move(data);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_explore::send_text(const request& request,
    std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    string_response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(text));
    response.body() = std::move(hexidecimal);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
