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
#include <bitcoin/node/settings.hpp>

#include <ranges>
#include <optional>
#include <variant>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

template <typename Number>
static bool to_number(Number& out, const std::string_view& token) NOEXCEPT
{
    return !token.empty() && is_ascii_numeric(token) && token.front() != '0' &&
        deserialize(out, token);
}

static hash_cptr to_hash(const std::string_view& token) NOEXCEPT
{
    hash_digest out{};
    return decode_hash(out, token) ?
        emplace_shared<const hash_digest>(std::move(out)) : hash_cptr{};
}

code path_to_request(request_t& out, const std::string& path) NOEXCEPT
{
    if (path.empty())
        return error::empty_path;

    // Avoid conflict with node type.
    using object_t = network::rpc::object_t;

    // Initialize json-rpc.v2 named params message.
    out = request_t
    {
        .jsonrpc = version::v2,
        .id = null_t{},
        .method = {},
        .params = object_t{}
    };

    auto& method = out.method;
    auto& params = std::get<object_t>(out.params.value());
    const auto segments = split(path, "/", false, true);
    BC_ASSERT(!segments.empty());

    size_t segment{};
    if (!segments[segment].starts_with('v'))
        return error::missing_version;

    uint8_t version{};
    if (!to_number(version, segments[segment++].substr(one)))
        return error::invalid_number;

    params["version"] = version;
    if (segment == segments.size())
        return error::missing_target;

    // transaction, address, inputs, and outputs are identical excluding names;
    // input and output are identical excluding names; block is unique.
    const auto target = segments[segment++];
    if (target == "transaction")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        method = "transaction";
        params["hash"] = hash;
    }
    else if (target == "address")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        method = "address";
        params["hash"] = hash;
    }
    else if (target == "inputs")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        method = "inputs";
        params["hash"] = hash;
    }
    else if (target == "outputs")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        method = "outputs";
        params["hash"] = hash;
    }
    else if (target == "input")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
            return error::missing_component;

        const auto component = segments[segment++];
        if (component == "scripts")
        {
            method = "input_scripts";
        }
        else if (component == "witnesses")
        {
            method = "input_witnesses";
        }
        else
        {
            uint32_t index{};
            if (!to_number(index, component))
                return error::invalid_number;

            params["index"] = index;
            if (segment == segments.size())
            {
                method = "input";
            }
            else
            {
                auto subcomponent = segments[segment++];
                if (subcomponent == "script")
                    method = "input_script";
                else if (subcomponent == "witness")
                    method = "input_witness";
                else
                    return error::invalid_subcomponent;
            }
        }
    }
    else if (target == "output")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
            return error::missing_component;

        const auto component = segments[segment++];
        if (component == "scripts")
        {
            method = "output_scripts";
        }
        else if (component == "spenders")
        {
            method = "output_spenders";
        }
        else
        {
            uint32_t index{};
            if (!to_number(index, component))
                return error::invalid_number;

            params["index"] = index;
            if (segment == segments.size())
            {
                method = "output";
            }
            else
            {
                auto subcomponent = segments[segment++];
                if (subcomponent == "script")
                    method = "output_script";
                else if (subcomponent == "spender")
                    method = "output_spender";
                else
                    return error::invalid_subcomponent;
            }
        }
    }
    else if (target == "block")
    {
        if (segment == segments.size())
            return error::missing_id_type;

        const auto by = segments[segment++];
        if (by == "hash")
        {
            if (segment == segments.size())
                return error::missing_hash;

            const auto hash = to_hash(segments[segment++]);
            if (!hash) return error::invalid_hash;

            // nullables can be implicit.
            ////params["height"] = null_t{};
            params["hash"] = hash;
        }
        else if (by == "height")
        {
            if (segment == segments.size())
                return error::missing_height;

            uint32_t height{};
            if (!to_number(height, segments[segment++]))
                return error::invalid_number;

            // nullables can be implicit.
            ////params["hash"] = null_t{};
            params["height"] = height;
        }
        else
        {
            return error::invalid_id_type;
        }

        if (segment == segments.size())
        {
            method = "block";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "transaction")
            {
                if (segment == segments.size())
                    return error::missing_position;

                uint32_t position{};
                if (!to_number(position, segments[segment++]))
                    return error::invalid_number;

                params["position"] = position;
                method = "block_tx";
            }
            else if (component == "header")
                method = "header";
            else if (component == "filter")
                method = "filter";
            else if (component == "transactions")
                method = "block_txs";
            else
                return error::invalid_component;
        }
    }
    else
    {
        return error::invalid_target;
    }

    return segment == segments.size() ? error::success : error::extra_segment;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
