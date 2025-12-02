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

#include <algorithm>
#include <optional>
#include <ranges>
#include <utility>
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
using namespace network::messages::peer;
using namespace std::placeholders;
using namespace boost::json;

// Avoiding namespace conflict.
using object_type = network::rpc::object_t;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_explore::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_EXPLORE(handle_get_block, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_EXPLORE(handle_get_header, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_block_txs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_block_tx, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_EXPLORE(handle_get_transaction, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_tx_block, _1, _2, _3, _4, _5);

    SUBSCRIBE_EXPLORE(handle_get_inputs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_input, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_EXPLORE(handle_get_input_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_input_witness, _1, _2, _3, _4, _5, _6);

    SUBSCRIBE_EXPLORE(handle_get_outputs, _1, _2, _3, _4, _5);
    SUBSCRIBE_EXPLORE(handle_get_output, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_output_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_output_spender, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_EXPLORE(handle_get_output_spenders, _1, _2, _3, _4, _5, _6);

    SUBSCRIBE_EXPLORE(handle_get_address, _1, _2, _3, _4, _5);
    SUBSCRIBE_EXPLORE(handle_get_filter, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_EXPLORE(handle_get_filter_hash, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_EXPLORE(handle_get_filter_header, _1, _2, _3, _4, _5, _6, _7);
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
    BC_ASSERT(stranded());

    request_t model{};
    if (LOG_ONLY(const auto ec =) parse_target(model, request.target()))
    {
        LOGA("Request parse [" << request.target() << "] " << ec.message());
        return false;
    }

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

constexpr auto data = to_value(media_type::application_octet_stream);
constexpr auto json = to_value(media_type::application_json);
constexpr auto text = to_value(media_type::text_plain);

bool protocol_explore::handle_get_block(const code& ec, interface::block,
    uint8_t, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (const auto ptr = archive().get_block(to_header(height, hash), witness))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(witness));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(witness));
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_header(const code& ec, interface::header,
    uint8_t, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (const auto ptr = archive().get_header(to_header(height, hash)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data());
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * chain::header::serialized_size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_block_txs(const code& ec,
    interface::block_txs, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto hashes = query.get_tx_keys(to_header(height, hash));
        !hashes.empty())
    {
        const auto size = hashes.size() * hash_size;

        switch (media)
        {
            case data:
            case text:
            {
                const auto data = pointer_cast<const uint8_t>(hashes.data());
                send_wire(media, to_chunk({ data, std::next(data, size) }));
                return true;
            }
            case json:
            {
                array out(hashes.size());
                std::ranges::transform(hashes, out.begin(),
                    [](const auto& hash) { return encode_base16(hash); });
                send_json({}, out, two * size);
                return true;
            }
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_block_tx(const code& ec, interface::block_tx,
    uint8_t, uint8_t media, uint32_t position, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_transaction(query.to_transaction(
        to_header(height, hash), position), witness))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(witness));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(witness));
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_transaction(const code& ec,
    interface::transaction, uint8_t, uint8_t media, const hash_cptr& hash,
    bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_transaction(query.to_tx(*hash), witness))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(witness));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(witness));
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_tx_block(const code& ec, interface::tx_block,
    uint8_t, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto block = query.to_block(query.to_tx(*hash));
    if (block.is_terminal())
    {
        send_not_found({});
        return true;
    }

    const auto key = query.get_header_key(block);
    if (key == null_hash)
    {
        send_internal_server_error({}, database::error::integrity);
        return true;
    }

    switch (media)
    {
        case data:
        case text:
            // TODO: serialize key to buffer.
            send_wire(media, data_chunk{});
            return true;
        case json:
            // TODO: json base16 key.
            return true;
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_inputs(const code& ec, interface::inputs,
    uint8_t, uint8_t media, const hash_cptr& hash, bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto tx = query.to_tx(*hash);
    if (tx.is_terminal())
    {
        send_not_found({});
        return true;
    }

    const auto ptr = query.get_inputs(tx, false);
    if (!ptr || ptr->empty())
    {
        send_internal_server_error({}, database::error::integrity);
        return true;
    }

    // TODO: iterate, naturally sorted by position.
    switch (media)
    {
        case data:
        case text:
            send_wire(media, ptr->front()->to_data());
            return true;
        case json:
            send_json({}, value_from(ptr->front()),
                two * ptr->front()->serialized_size(false));
            return true;
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_input(const code& ec, interface::input,
    uint8_t, uint8_t media, const hash_cptr& hash, uint32_t index,
    bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_input(query.to_tx(*hash), index, false))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data());
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(false));
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_input_script(const code& ec,
    interface::input_script, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_input_script(query.to_point(
        query.to_tx(*hash), index)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(false));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(false));
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_input_witness(const code& ec,
    interface::input_witness, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_witness(query.to_point(
        query.to_tx(*hash), index)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(false));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(false));
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_outputs(const code& ec, interface::outputs,
    uint8_t, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();

    // TODO: iterate, naturally sorted by position.
    if (const auto ptr = query.get_outputs(query.to_tx(*hash)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->front()->to_data());
                return true;
            case json:
                send_json({}, value_from(ptr->front()),
                    two * ptr->front()->serialized_size());
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_output(const code& ec, interface::output,
    uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_output(query.to_tx(*hash), index))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data());
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size());
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_output_script(const code& ec,
    interface::output_script, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto ptr = query.get_output_script(query.to_output(
        query.to_tx(*hash), index)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data(false));
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size(false));
            return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_output_spender(const code& ec,
    interface::output_spender, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // TODO: query only confirmed spender.
    const auto& query = archive();
    const auto spenders = query.to_spenders(*hash, index);
    if (spenders.empty())
    {
        send_not_found({});
        return true;
    }
    
    if (const auto point = query.get_point(spenders.front());
        point.hash() != null_hash)
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, point.to_data());
                return true;
            case json:
                send_json({}, value_from(point),
                    two * point.serialized_size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_output_spenders(const code& ec,
    interface::output_spender, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // TODO: iterate, sort by height/position/index.
    const auto& query = archive();
    const auto spenders = query.to_spenders(*hash, index);
    if (spenders.empty())
    {
        send_not_found({});
        return true;
    }

    // TODO: iterate, sort in query.
    if (const auto point = query.get_point(spenders.front());
        point.hash() != null_hash)
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, point.to_data());
                return true;
            case json:
                send_json({}, value_from(point),
                    two * point.serialized_size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_address(const code& ec, interface::address,
    uint8_t, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.address_enabled())
    {
        send_not_implemented({});
        return true;
    }

    // TODO: iterate, sort by height/position/index.
    database::output_links outputs{};
    if (!query.to_address_outputs(outputs, *hash))
    {
        send_internal_server_error({}, database::error::integrity);
        return true;
    }

    if (outputs.empty())
    {
        send_not_found({});
        return true;
    }

    // TODO: iterate, sort in query.
    if (const auto ptr = query.get_output(outputs.front()))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, ptr->to_data());
                return true;
            case json:
                send_json({}, value_from(ptr),
                    two * ptr->serialized_size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_filter(const code& ec, interface::filter,
    uint8_t, uint8_t media, uint8_t type,
    std::optional<system::hash_cptr> hash,
    std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_not_implemented({});
        return true;
    }

    if (type != client_filter::type_id::neutrino)
    {
        send_not_found({});
        return true;
    }

    data_chunk filter{};
    if (query.get_filter_body(filter, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, std::move(filter));
                return true;
            case json:
                const auto base16 = encode_base16(filter);
                send_json({}, value_from(base16), base16.size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_filter_hash(const code& ec,
    interface::filter_hash, uint8_t, uint8_t media, uint8_t type,
    std::optional<system::hash_cptr> hash,
    std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_not_implemented({});
        return true;
    }

    if (type != client_filter::type_id::neutrino)
    {
        send_not_found({});
        return true;
    }

    data_chunk chunk{ hash_size };
    auto& filter_hash = unsafe_array_cast<uint8_t, hash_size>(chunk.data());
    if (query.get_filter_hash(filter_hash, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, std::move(chunk));
                return true;
            case json:
                const auto base16 = encode_base16(chunk);
                send_json({}, value_from(base16), base16.size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

bool protocol_explore::handle_get_filter_header(const code& ec,
    interface::filter_header, uint8_t, uint8_t media, uint8_t type,
    std::optional<system::hash_cptr> hash,
    std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_not_implemented({});
        return true;
    }

    if (type != client_filter::type_id::neutrino)
    {
        send_not_found({});
        return true;
    }

    data_chunk chunk{ hash_size };
    auto& filter_head = unsafe_array_cast<uint8_t, hash_size>(chunk.data());
    if (query.get_filter_head(filter_head, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
            case text:
                send_wire(media, std::move(chunk));
                return true;
            case json:
                const auto base16 = encode_base16(chunk);
                send_json({}, value_from(base16), base16.size());
                return true;
        }
    }

    send_not_found({});
    return true;
}

// private
// ----------------------------------------------------------------------------

void protocol_explore::send_wire(uint8_t media, data_chunk&& chunk) NOEXCEPT
{
    if (media == data)
        send_chunk({}, std::move(chunk));
    else
        send_text({}, encode_base16(chunk));
}

database::header_link protocol_explore::to_header(
    const std::optional<uint32_t>& height,
    const std::optional<hash_cptr>& hash) NOEXCEPT
{
    const auto& query = archive();

    if (hash.has_value())
        return query.to_header(*(hash.value()));

    if (height.has_value())
        return query.to_confirmed(height.value());

    return {};
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef SUBSCRIBE_EXPLORE

} // namespace node
} // namespace libbitcoin
