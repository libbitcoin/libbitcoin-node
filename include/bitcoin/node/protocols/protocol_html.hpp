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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HTML_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HTML_HPP

#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {
    
/// Abstract base for HTML protocols, thread safe.
class BCN_API protocol_html
  : public network::protocol_http,
    public node::protocol
{
public:
    /// http channel, but html settings.
    using options_t = server::settings::html_server;
    using channel_t = node::channel_http;

protected:
    protocol_html(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : network::protocol_http(session, channel, options),
        options_(options),
        node::protocol(session, channel)
    {
    }

    /// Message handlers by http method.
    void handle_receive_get(const code& ec,
        const network::http::method::get& request) NOEXCEPT override;

    /// Dispatch.
    virtual bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT;
    virtual void dispatch_file(
        const network::http::request& request) NOEXCEPT;
    virtual void dispatch_embedded(
        const network::http::request& request) NOEXCEPT;

    /// Senders.
    virtual void send_json(const network::http::request& request,
        boost::json::value&& model, size_t size_hint) NOEXCEPT;
    virtual void send_text(const network::http::request& request,
        std::string&& hexidecimal) NOEXCEPT;
    virtual void send_data(const network::http::request& request,
        system::data_chunk&& bytes) NOEXCEPT;
    virtual void send_file(const network::http::request& request,
        network::http::file&& file, network::http::mime_type type) NOEXCEPT;
    virtual void send_span(const network::http::request& request,
        network::http::span_body::value_type&& span,
        network::http::mime_type type) NOEXCEPT;
    virtual void send_buffer(const network::http::request& request,
        network::http::buffer_body::value_type&& buffer,
        network::http::mime_type type) NOEXCEPT;

    /// Utilities.
    bool is_allowed_origin(const network::http::fields& fields,
        size_t version) const NOEXCEPT;
    std::filesystem::path to_path(
        const std::string& target = "/") const NOEXCEPT;
    std::filesystem::path to_local_path(
        const std::string& target = "/") const NOEXCEPT;

private:
    // This is thread safe.
    const options_t& options_;
};

} // namespace node
} // namespace libbitcoin

#endif
