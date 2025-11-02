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
#include <bitcoin/node/protocols/protocol_html.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_html

using namespace network::http;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Handle get method.
// ----------------------------------------------------------------------------

void protocol_html::handle_receive_get(const code& ec,
    const method::get& request) NOEXCEPT
{
    BC_ASSERT(stranded());

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

    // Embedded page site.
    if (dispatch_embedded(*request))
        return;

    // Empty path implies malformed target (terminal).
    const auto path = to_local_path(request->target());
    if (path.empty())
    {
        // TODO: split out sanitize from canonicalize so that this can return
        // send_not_found() when the request is sanitary but not found.
        send_bad_target(*request);
        return;
    }

    // Not open implies file not found (non-terminal).
    auto file = get_file_body(path);
    if (!file.is_open())
    {
        send_not_found(*request);
        return;
    }

    send_file(*request, std::move(file),
        file_mime_type(path, mime_type::application_octet_stream));
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_html::dispatch_embedded(const request& request) NOEXCEPT
{
    // False only if not enabled, otherwise handled below.
    if (!options_.pages.enabled())
        return false;

    const auto& pages = config().server.explore.pages;
    switch (const auto mime = file_mime_type(to_path(request.target())))
    {
        case mime_type::text_css:
            send_span(request, pages.css(), mime);
            return true;
        case mime_type::text_html:
            send_span(request, pages.html(), mime);
            return true;
        case mime_type::application_javascript:
            send_span(request, pages.ecma(), mime);
            return true;
        case mime_type::font_woff:
        case mime_type::font_woff2:
            send_span(request, pages.font(), mime);
            return true;
        case mime_type::image_png:
        case mime_type::image_gif:
        case mime_type::image_jpeg:
            send_span(request, pages.icon(), mime);
            return true;
        default:
            return false;
    }
}

// Senders.
// ----------------------------------------------------------------------------

constexpr auto data = mime_type::application_octet_stream;
constexpr auto json = mime_type::application_json;
constexpr auto text = mime_type::text_plain;

void protocol_html::send_json(const request& request,
    boost::json::value&& model, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(json));
    response.body() = { std::move(model), size_hint };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_text(const request& request,
    std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(text));
    response.body() = std::move(hexidecimal);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_data(const request& request,
    system::data_chunk&& bytes) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(data));
    response.body() = std::move(bytes);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_file(const request& request, file&& file,
    mime_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(file.is_open(), "sending closed file handle");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(type));
    response.body() = std::move(file);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_span(const request& request,
    span_body::value_type&& span, mime_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(type));
    response.body() = std::move(span);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

// Utilities.
// ----------------------------------------------------------------------------

bool protocol_html::is_allowed_origin(const fields& fields,
    size_t version) const NOEXCEPT
{
    // Allow same-origin and no-origin requests.
    // Origin header field is not available until http 1.1.
    const auto origin = fields[field::origin];
    if (origin.empty() || version < version_1_1)
        return true;

    return options_.origins.empty() || system::contains(options_.origins,
        network::config::to_normal_host(origin, default_port()));
}

std::filesystem::path protocol_html::to_path(
    const std::string& target) const NOEXCEPT
{
    return target == "/" ? target + options_.default_ : target;
}

std::filesystem::path protocol_html::to_local_path(
    const std::string& target) const NOEXCEPT
{
    return sanitize_origin(options_.path, to_path(target).string());
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
