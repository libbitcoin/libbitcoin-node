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
    const method::get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http origin form for get.
    if (!is_origin_form(get->target()))
    {
        send_bad_target(*get);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*get, get->version()))
    {
        send_forbidden(*get);
        return;
    }

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*get, get->version()))
    {
        send_bad_host(*get);
        return;
    }

    // Always try API dispatch, false if unhandled.
    if (try_dispatch_object(*get))
        return;

    // Require file system dispatch if path is configured (always handles).
    if (!options_.path.empty())
    {
        dispatch_file(*get);
        return;
    }

    // Require embedded dispatch if site is configured (always handles).
    if (options_.pages.enabled())
    {
        dispatch_embedded(*get);
        return;
    }

    // Neither site is enabled and object dispatch doesn't support.
    send_not_implemented(*get);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_html::try_dispatch_object(const request&) NOEXCEPT
{
    return false;
}

void protocol_html::dispatch_embedded(const request& request) NOEXCEPT
{
    const auto& pages = config().server.explore.pages;
    switch (const auto mime = file_mime_type(to_path(request.target())))
    {
        case mime_type::text_css:
            send_span(request, pages.css(), mime);
            break;
        case mime_type::text_html:
            send_span(request, pages.html(), mime);
            break;
        case mime_type::application_javascript:
            send_span(request, pages.ecma(), mime);
            break;
        case mime_type::font_woff:
        case mime_type::font_woff2:
            send_span(request, pages.font(), mime);
            break;
        case mime_type::image_png:
        case mime_type::image_gif:
        case mime_type::image_jpeg:
            send_span(request, pages.icon(), mime);
            break;
        default:
            send_not_found(request);
    }
}

void protocol_html::dispatch_file(const request& request) NOEXCEPT
{
    // Empty path implies malformed target (terminal).
    auto path = to_local_path(request.target());
    if (path.empty())
    {
        send_bad_target(request);
        return;
    }

    // If no file extension it's REST on the single/default html page.
    if (!path.has_extension())
    {
        path = to_local_path();

        // Default html page (e.g. index.html) is not configured.
        if (path.empty())
        {
            send_not_implemented(request);
            return;
        }
    }

    // Get the single/default or explicitly requested page.
    auto file = get_file_body(path);
    if (!file.is_open())
    {
        send_not_found(request);
        return;
    }

    const auto octet_stream = mime_type::application_octet_stream;
    send_file(request, std::move(file), file_mime_type(path, octet_stream));
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

void protocol_html::send_buffer(const request& request,
    buffer_body::value_type&& buffer, mime_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(type));
    response.body() = std::move(buffer);
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
