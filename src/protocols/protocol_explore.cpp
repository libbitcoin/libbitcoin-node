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

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_explore

using namespace system;
using namespace network::http;
using namespace std::placeholders;

// Handle get method.
// ----------------------------------------------------------------------------

// fields[], request->target(), file.is_open() are undeclared noexcept.
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

////void protocol_explore::handle_receive_get(const code& ec,
////    const method::get& request) NOEXCEPT
////{
////    BC_ASSERT_MSG(stranded(), "strand");
////
////    if (stopped(ec))
////        return;
////
////    // Enforce http origin policy (requires configured hosts).
////    if (!is_allowed_origin((*request)[field::origin], request->version()))
////    {
////        send_forbidden(*request);
////        return;
////    }
////
////    // Enforce http host header (if any hosts are configured).
////    if (!is_allowed_host((*request)[field::host], request->version()))
////    {
////        send_bad_host(*request);
////        return;
////    }
////
////    // Empty path implies malformed target (terminal).
////    auto path = to_local_path(request->target());
////    if (path.empty())
////    {
////        send_bad_target(*request);
////        return;
////    }
////
////    if (!path.has_extension())
////    {
////        // Empty path implies default page is invalid or not configured.
////        path = to_local_path();
////        if (path.empty())
////        {
////            send_not_implemented(*request);
////            return;
////        }
////    }
////
////    // Not open implies file not found (non-terminal).
////    auto file = get_file_body(path);
////    if (!file.is_open())
////    {
////        send_not_found(*request);
////        return;
////    }
////
////    send_file(*request, std::move(file), file_mime_type(path));
////}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
