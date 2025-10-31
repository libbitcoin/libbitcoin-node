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
// fields[] are undeclared noexcept.

void protocol_html::handle_receive_get(const code& ec,
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

    // Empty path implies malformed target (terminal).
    const auto path = to_local_path(request->target());
    if (path.empty())
    {
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

    const auto default_type = mime_type::application_octet_stream;
    send_file(*request, std::move(file), file_mime_type(path, default_type));
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_html::send_file(const request& request, file&& file,
    mime_type type) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");
    BC_ASSERT_MSG(file.is_open(), "sending closed file handle");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    response.set(field::content_type, from_mime_type(type));
    response.body() = std::move(file);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

// Utilities.
// ----------------------------------------------------------------------------

bool protocol_html::is_allowed_origin(const fields& fields,
    size_t version) const NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");

    // Allow same-origin and no-origin requests.
    // Origin header field is not available until http 1.1.
    const auto origin = fields[field::origin];
    if (origin.empty() || version < version_1_1)
        return true;

    return options_.origins.empty() || system::contains(options_.origins,
        network::config::to_normal_host(origin, default_port()));
}

std::filesystem::path protocol_html::to_local_path(
    const std::string& target) const NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "strand");

    return sanitize_origin(options_.path,
        target == "/" ? target + options_.default_ : target);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
