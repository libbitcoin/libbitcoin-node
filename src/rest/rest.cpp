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

template <typename Number>
Number to_number(const std::string_view& token) THROWS
{
    Number out{};
    if (token.empty() || token.front() == '0' ||
        !is_ascii_numeric(token) || !deserialize(out, token))
        throw std::runtime_error("invalid number");

    return out;
}

hash_cptr to_hash(const std::string_view& token) THROWS
{
    hash_digest out{};
    if (!decode_hash(out, token))
        throw std::runtime_error("invalid hash");

    return emplace_shared<const hash_digest>(std::move(out));
}

request_t path_to_request(const std::string& url_path) THROWS
{
    // Avoid conflict with node type.
    using object_t = network::rpc::object_t;

    // Initialize json-rpc.v2 named params message.
    request_t request
    {
        .jsonrpc = version::v2,
        .id = null_t{},
        .method = {},
        .params = object_t{}
    };

    auto& method = request.method;
    auto& params = std::get<object_t>(request.params.value());
    const auto segments = split(url_path, "/", false, true);
    if (segments.empty())
        throw std::runtime_error("empty url path");

    size_t index{};
    if (!segments[index].starts_with('v'))
        throw std::runtime_error("missing version");

    params["version"] = to_number<uint8_t>(segments[index++].substr(one));
    if (index == segments.size())
        throw std::runtime_error("missing target");

    // transaction, address, inputs, and outputs are identical excluding names;
    // input and output are identical excluding names; block is unique.
    const auto target = segments[index++];
    if (target == "transaction")
    {
        if (index == segments.size())
            throw std::runtime_error("missing transaction hash");

        method = "transaction";
        params["hash"] = to_hash(segments[index++]);
    }
    else if (target == "address")
    {
        if (index == segments.size())
            throw std::runtime_error("missing address hash");

        method = "address";
        params["hash"] = to_hash(segments[index++]);
    }
    else if (target == "inputs")
    {
        if (index == segments.size())
            throw std::runtime_error("missing inputs tx hash");

        method = "inputs";
        params["hash"] = to_hash(segments[index++]);
    }
    else if (target == "outputs")
    {
        if (index == segments.size())
            throw std::runtime_error("missing outputs tx hash");

        method = "outputs";
        params["hash"] = to_hash(segments[index++]);
    }
    else if (target == "input")
    {
        if (index == segments.size())
            throw std::runtime_error("missing input tx hash");

        params["hash"] = to_hash(segments[index++]);
        if (index == segments.size())
            throw std::runtime_error("missing input component");

        const auto component = segments[index++];
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
            params["index"] = to_number<uint32_t>(component);
            if (index == segments.size())
            {
                method = "input";
            }
            else
            {
                auto subcomponent = segments[index++];
                if (subcomponent == "script")
                    method = "input_script";
                else if (subcomponent == "witness")
                    method = "input_witness";
                else
                    throw std::runtime_error("unexpected input subcomponent");
            }
        }
    }
    else if (target == "output")
    {
        if (index == segments.size())
            throw std::runtime_error("missing output tx hash");

        params["hash"] = to_hash(segments[index++]);
        if (index == segments.size())
            throw std::runtime_error("missing output component");

        const auto component = segments[index++];
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
            params["index"] = to_number<uint32_t>(component);
            if (index == segments.size())
            {
                method = "output";
            }
            else
            {
                auto subcomponent = segments[index++];
                if (subcomponent == "script")
                    method = "output_script";
                else if (subcomponent == "spender")
                    method = "output_spender";
                else
                    throw std::runtime_error("unexpected output subcomponent");
            }
        }
    }
    else if (target == "block")
    {
        if (index == segments.size())
            throw std::runtime_error("missing block id");

        const auto by = segments[index++];
        if (by == "hash")
        {
            if (index == segments.size())
                throw std::runtime_error("missing block hash");

            // height nullable (required but can be set to null_t).
            params["hash"] = to_hash(segments[index++]);
            params["height"] = null_t{};
        }
        else if (by == "height")
        {
            if (index == segments.size())
                throw std::runtime_error("missing block height");

            // hash required shared_ptr<T> and cannot be nullptr (or null_t).
            params["hash"] = to_shared(null_hash);
            params["height"] = to_number<uint32_t>(segments[index++]);
        }
        else
        {
            throw std::runtime_error("invalid block id");
        }

        if (index == segments.size())
        {
            method = "block";
        }
        else
        {
            const auto component = segments[index++];
            if (component == "header")
                method = "header";
            else if (component == "filter")
                method = "filter";
            else if (component == "transactions")
                method = "block_txs";
            else if (component != "transaction")
                throw std::runtime_error("invalid block component");

            if (index == segments.size())
                throw std::runtime_error("missing tx position");

            params["position"] = to_number<uint32_t>(segments[index++]);
            method = "block_tx";
        }
    }
    else
    {
        throw std::runtime_error("unknown target");
    }

    if (index != segments.size())
        throw std::runtime_error("extra segments");

    return request;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
