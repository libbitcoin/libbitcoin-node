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
/// To keep inheritance simple this derives from protocol_ws, which in turn
/// derives from protocol_http (as required). So any html server is able to
/// operate as a websocket server.
class BCN_API protocol_html
  : public node::protocol,
    public network::protocol_ws,
    protected network::tracker<protocol_html>
{
public:
    using options_t = server::settings::html_server;
    using channel_t = node::channel_ws;

protected:
    inline protocol_html(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol_ws(session, channel, options),
        options_(options),
        network::tracker<protocol_html>(session->log)
    {
    }

    /// Message handlers by http method.
    void handle_receive_get(const code& ec,
        const network::http::method::get::cptr& get) NOEXCEPT override;

    /// Dispatch.
    virtual bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT;
    virtual void dispatch_file(
        const network::http::request& request) NOEXCEPT;
    virtual void dispatch_embedded(
        const network::http::request& request) NOEXCEPT;

    /// Senders.
    virtual void send_json(boost::json::value&& model, size_t size_hint,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_text(std::string&& hexidecimal,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_chunk(system::data_chunk&& bytes,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_file(network::http::file&& file,
        network::http::media_type type,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_span(network::http::span_body::value_type&& span,
        network::http::media_type type,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_buffer(network::http::buffer_body::value_type&& buffer,
        network::http::media_type type,
        const network::http::request& request={}) NOEXCEPT;

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
