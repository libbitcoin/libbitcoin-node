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

#include <optional>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/parse/parse.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_explore

#define SUBSCRIBE_EXPLORE(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)
    
using namespace system;
using namespace network::rpc;
using namespace network::http;
using namespace std::placeholders;

// Avoiding namespace conflict.
using object_type = network::rpc::object_t;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start.
// ----------------------------------------------------------------------------

void protocol_explore::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_EXPLORE(handle_get_block, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_EXPLORE(handle_get_header, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_filter, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_block_txs, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_block_tx, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_EXPLORE(handle_get_transaction, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_address, _1, _2, _3, _4, _5);
    ////SUBSCRIBE_EXPLORE(handle_get_input, _1, _2, _3, _4, _5, _6, _7);
    ////SUBSCRIBE_EXPLORE(handle_get_input_script, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_input_witness, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_output, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_output_script, _1, _2, _3, _4, _5, _6);
    ////SUBSCRIBE_EXPLORE(handle_get_output_spender, _1, _2, _3, _4, _5, _6);
    protocol_html::start();
}

void protocol_explore::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    dispatcher_.stop(ec);
    protocol_html::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_explore::try_dispatch_object(const request& request) NOEXCEPT
{
    request_t model{};
    if (const auto ec = parse_target(model, request.target()))
        return false;

    if (!parse_query(model, request))
    {
        send_not_acceptable(request);
        return true;
    }

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(request, ec);

    return true;
}

// Handlers.
// ----------------------------------------------------------------------------

bool protocol_explore::handle_get_block(const code& ec, interface::block,
    uint8_t, uint8_t media, std::optional<system::hash_cptr> hash,
    std::optional<uint32_t> height, bool witness) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = hash.has_value() ? query.to_header(*(hash.value())) :
        query.to_confirmed(height.value());

    // TODO: there's no request.
    const network::http::request request{};

    if (const auto ptr = query.get_block(link, witness))
    {
        switch (media)
        {
            case to_value(media_type::application_octet_stream):
                send_data(request, ptr->to_data(witness));
                return true;
            case to_value(media_type::text_plain):
                send_text(request, encode_base16(ptr->to_data(witness)));
                return true;
            case to_value(media_type::application_json):
                send_json(request, value_from(ptr),
                    ptr->serialized_size(witness));
                return true;
        }
    }

    send_not_found(request);
    return true;
}

bool protocol_explore::handle_get_header(const code& ec, interface::header,
    uint8_t, uint8_t media, std::optional<system::hash_cptr> hash,
    std::optional<uint32_t> height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = hash.has_value() ? query.to_header(*(hash.value())) :
        query.to_confirmed(height.value());

    // TODO: there's no request.
    const network::http::request request{};

    if (const auto ptr = query.get_header(link))
    {
        switch (media)
        {
            case to_value(media_type::application_octet_stream):
                send_data(request, ptr->to_data());
                return true;
            case to_value(media_type::text_plain):
                send_text(request, encode_base16(ptr->to_data()));
                return true;
            case to_value(media_type::application_json):
                send_json(request, value_from(ptr),
                    chain::header::serialized_size());
                return true;
        }
    }

    send_not_found(request);
    return true;
}

bool protocol_explore::handle_get_transaction(const code& ec,
    interface::transaction, uint8_t, uint8_t media, system::hash_cptr hash,
    bool witness) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();

    // TODO: there's no request.
    const network::http::request request{};

    if (const auto ptr = query.get_transaction(query.to_tx(*hash), witness))
    {
        switch (media)
        {
            case to_value(media_type::application_octet_stream):
                send_data(request, ptr->to_data(witness));
                return true;
            case to_value(media_type::text_plain):
                send_text(request, encode_base16(ptr->to_data(witness)));
                return true;
            case to_value(media_type::application_json):
                send_json(request, value_from(ptr),
                    ptr->serialized_size(witness));
                return true;
        }
    }

    send_not_found(request);
    return true;
}

BC_POP_WARNING()

#undef SUBSCRIBE_EXPLORE

} // namespace node
} // namespace libbitcoin
