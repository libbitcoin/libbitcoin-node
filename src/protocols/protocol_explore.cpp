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

const auto data = mime_type::application_octet_stream;
const auto json = mime_type::application_json;
const auto text = mime_type::text_plain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Handle get method.
// ----------------------------------------------------------------------------
// TODO: performance timing header.
// TODO: formatted error responses.
// TODO: formatted error responses.
// TODO: priority accept media type sort and dispatch.
// TODO: URI path parse (see API doc).

/// TODO: move to own source.
/// Pagination and filtering are via query string.
enum targets
{
    /// /v[]/block/hash/[bkhash] {1}
    /// /v[]/block/height/[height] {1}
    block,

    /// /v[]/block/hash/[bkhash]/filter {1}
    /// /v[]/block/height/[height]/filter {1}
    filter,

    /// /v[]/block/hash/[bkhash]/header {1}
    /// /v[]/block/height/[height]/header {1}
    header,

    /// /v[]/transaction/hash/[txhash] {1}
    /// /v[]/block/hash/[bkhash]/transaction/position/[position] {1}
    /// /v[]/block/height/[height]/transaction/position/[position] {1}
    transaction,

    /// /v[]/block/hash/[bkhash]/transactions {all txs in the block}
    /// /v[]/block/height/[height]/transactions {all txs in the block}
    transactions,

    // --------------------------------------------------------------------

    /// /v[]/input/[txhash]/[index] {1}
    input,

    /// /v[]/inputs/[txhash] {all inputs in the tx}
    inputs,

    /// /v[]/input/[txhash]/[index]/script {1}
    input_script,

    /// /v[]/input/[txhash]/scripts {all input scripts in the tx}
    input_scripts,

    /// /v[]/input/[txhash]/[index]/witness {1}
    input_witness,

    /// /v[]/input/[txhash]/witnesses {all witnesses in the tx}
    input_witnesses,

    // --------------------------------------------------------------------

    /// /v[]/output/[txhash]/[index] {1}
    output,

    /// /v[]/outputs/[txhash] {all outputs in the tx}
    outputs,

    /// /v[]/output/[txhash]/[index]/script {1}
    output_script,

    /// /v[]/output/[txhash]/scripts {all output scripts in the tx}
    output_scripts,

    /// /v[]/output/[txhash]/[index]/spender {1 - confirmed}
    output_spender,

    /// /v[]/output/[txhash]/spenders {all}
    output_spenders,

    // --------------------------------------------------------------------

    /// /v[]/address/[output-script-hash] {all}
    address
};

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

    if (const auto parts = split(uri.path(), "/");
        parts.size() == two)
    {
        const auto hd = parts.front() == "header"      || parts.front() == "hd";
        const auto bk = parts.front() == "block"       || parts.front() == "bk";
        const auto tx = parts.front() == "transaction" || parts.front() == "tx";
        if (!hd && !bk && !tx)
        {
            send_bad_target(*request);
            return;
        }

        auto params = uri.decode_query();
        const auto format = params["format"];
        const auto accepts = to_mime_types((*request)[field::accept]);
        const auto is_json = contains(accepts, json) || format == "json";
        const auto is_text = contains(accepts, text) || format == "text";
        const auto is_data = contains(accepts, data) || format == "data";
        const auto wit = params["witness"] != "false";
        const auto hex = parts.back();

        hash_digest hash{};
        if ((is_json || is_text || is_data) && !decode_hash(hash, hex))
        {
            send_bad_target(*request);
            return;
        }

        const auto& query = archive();

        if (is_json)
        {
            if (hd)
            {
                if (const auto ptr = query.get_header(query.to_header(hash)))
                {
                    send_json(*request, value_from(ptr), ptr->serialized_size());
                    return;
                }
            }
            else if (bk)
            {
                if (const auto ptr = query.get_block(query.to_header(hash), wit))
                {
                    send_json(*request, value_from(ptr), ptr->serialized_size(wit));
                    return;
                }
            }
            else
            {
                if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
                {
                    send_json(*request, value_from(ptr), ptr->serialized_size(wit));
                    return;
                }
            }

            send_not_found(*request);
            return;
        }
        else if (is_text)
        {
            if (hd)
            {
                if (const auto ptr = query.get_header(query.to_header(hash)))
                {
                    send_text(*request, encode_base16(ptr->to_data()));
                    return;
                }
            }
            else if (bk)
            {
                if (const auto ptr = query.get_block(query.to_header(hash), wit))
                {
                    send_text(*request, encode_base16(ptr->to_data(wit)));
                    return;
                }
            }
            else
            {
                if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
                {
                    send_text(*request, encode_base16(ptr->to_data(wit)));
                    return;
                }
            }

            send_not_found(*request);
            return;
        }
        else if (is_data)
        {
            if (hd)
            {
                if (const auto ptr = query.get_header(query.to_header(hash)))
                {
                    send_data(*request, ptr->to_data());
                    return;
                }
            }
            else if (bk)
            {
                if (const auto ptr = query.get_block(query.to_header(hash), wit))
                {
                    send_data(*request, ptr->to_data(wit));
                    return;
                }
            }
            else
            {
                if (const auto ptr = query.get_transaction(query.to_tx(hash), wit))
                {
                    send_data(*request, ptr->to_data(wit));
                    return;
                }
            }

            send_not_found(*request);
            return;
        }
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
    boost::json::value&& model, size_t size_hint) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(json));
    response.body() = { std::move(model), size_hint };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_explore::send_text(const request& request,
    std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(text));
    response.body() = std::move(hexidecimal);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_explore::send_data(const request& request,
    data_chunk&& bytes) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(data));
    response.body() = std::move(bytes);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
