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
#include <bitcoin/node/protocols/protocol_bitcoind.hpp>

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_bitcoind
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network::rpc;
using namespace network::http;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind::start() NOEXCEPT
{
    using namespace std::placeholders;

    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_best_block_hash, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_chain_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_header, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_tx_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_work, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_out, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_set_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prune_block_chain, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_save_mem_pool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_tx_out_set, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_chain, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_tx_out_set, _1, _2, _3);
    protocol_http::start();
}

// Dispatch.
// ----------------------------------------------------------------------------

void protocol_bitcoind::handle_receive_options(const code& ec,
    const options::cptr& options) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*options, options->version()))
    {
        send_bad_host(*options);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*options, options->version()))
    {
        send_forbidden(*options);
        return;
    }

    send_ok(*options);
}

// TODO: also handle_receive_get and dispatch based on URL parse.
void protocol_bitcoind::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*post, post->version()))
    {
        send_bad_host(*post);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*post, post->version()))
    {
        send_forbidden(*post);
        return;
    }

    using json_t = json_body::value_type;
    const auto& body = post->body();
    if (!body.contains<json_t>())
    {
        send_not_acceptable(*post);
        return;
    }

    request_t request{};
    try
    {
        request = value_to<request_t>(body.get<json_t>().model);
    }
    catch (const boost::system::system_error& e)
    {
        send_bad_target(e.code(), *post);
        return;
    }
    catch (...)
    {
        send_bad_target(error::unexpected_parse, *post);
        return;
    }

    set_post(post);
    if (const auto code = dispatcher_.notify(request))
        stop(code);
}

// Handlers.
// ----------------------------------------------------------------------------
// github.com/bitcoin/bitcoin/blob/master/doc/JSON-RPC-interface.md
// TODO: precompute size for buffer hints.

// {"jsonrpc": "1.0", "id": "curltest", "method": "getbestblockhash", "params": []}
bool protocol_bitcoind::handle_get_best_block_hash(const code& ec,
    interface::get_best_block_hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto hash = query.get_header_key(query.to_confirmed(
        query.get_top_confirmed()));

    const response_t model{ .result = encode_hash(hash) };
    send_json(value_from(model), two * system::hash_size);
    return true;
}

// method<"getblock", string_t, optional<0_u32>>{ "blockhash", "verbosity" },
bool protocol_bitcoind::handle_get_block(const code& ec,
    interface::get_block, const std::string&, double) NOEXCEPT
{
    return !ec;
}

bool protocol_bitcoind::handle_get_block_chain_info(const code& ec,
    interface::get_block_chain_info) NOEXCEPT
{
    return !ec;
}

bool protocol_bitcoind::handle_get_block_count(const code& ec,
    interface::get_block_count) NOEXCEPT
{
    return !ec;
}

// method<"getblockfilter", string_t, optional<"basic"_t>>{ "blockhash", "filtertype" },
bool protocol_bitcoind::handle_get_block_filter(const code& ec,
    interface::get_block_filter, const std::string&, const std::string&) NOEXCEPT
{
    return !ec;
}

// method<"getblockhash", number_t>{ "height" },
bool protocol_bitcoind::handle_get_block_hash(const code& ec,
    interface::get_block_hash, network::rpc::number_t) NOEXCEPT
{
    return !ec;
}

// method<"getblockheader", string_t, optional<true>>{ "blockhash", "verbose" },
bool protocol_bitcoind::handle_get_block_header(const code& ec,
    interface::get_block_header, const std::string&, bool) NOEXCEPT
{
    return !ec;
}

// method<"getblockstats", string_t, optional<empty::array>>{ "hash_or_height", "stats" },
bool protocol_bitcoind::handle_get_block_stats(const code& ec,
    interface::get_block_stats, const std::string&,
    const network::rpc::array_t&) NOEXCEPT
{
    return !ec;
}

// method<"getchaintxstats", optional<-1_i32>, optional<""_t>>{ "nblocks", "blockhash" },
bool protocol_bitcoind::handle_get_chain_tx_stats(const code& ec,
    interface::get_chain_tx_stats, double, const std::string&) NOEXCEPT
{
    return !ec;
}

bool protocol_bitcoind::handle_get_chain_work(const code& ec,
    interface::get_chain_work) NOEXCEPT
{
    return !ec;
}

// method<"gettxout", string_t, number_t, optional<true>>{ "txid", "n", "include_mempool" },
bool protocol_bitcoind::handle_get_tx_out(const code& ec,
    interface::get_tx_out, const std::string&, double, bool) NOEXCEPT
{
    return !ec;
}

bool protocol_bitcoind::handle_get_tx_out_set_info(const code& ec,
    interface::get_tx_out_set_info) NOEXCEPT
{
    return !ec;
}

// method<"pruneblockchain", number_t>{ "height" },
bool protocol_bitcoind::handle_prune_block_chain(const code& ec,
    interface::prune_block_chain, double) NOEXCEPT
{
    return !ec;
}

bool protocol_bitcoind::handle_save_mem_pool(const code& ec,
    interface::save_mem_pool) NOEXCEPT
{
    return !ec;
}

// method<"scantxoutset", string_t, optional<empty::array>>{ "action", "scanobjects" },
bool protocol_bitcoind::handle_scan_tx_out_set(const code& ec,
    interface::scan_tx_out_set, const std::string&,
    const network::rpc::array_t&) NOEXCEPT
{
    return !ec;
}

// method<"verifychain", optional<4_u32>, optional<288_u32>>{ "checklevel", "nblocks" },
bool protocol_bitcoind::handle_verify_chain(const code& ec,
    interface::verify_chain, double, double) NOEXCEPT
{
    return !ec;
}

// method<"verifytxoutset", string_t>{ "input_verify_flag" },
bool protocol_bitcoind::handle_verify_tx_out_set(const code& ec,
    interface::verify_tx_out_set, const std::string&) NOEXCEPT
{
    return !ec;
}

// private
// ----------------------------------------------------------------------------

void protocol_bitcoind::set_post(const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(post);
    post_ = post;
}

const protocol_bitcoind::post& protocol_bitcoind::get_post() const NOEXCEPT
{
    BC_ASSERT(post_);
    return *post_;
}

// TODO: post-process response for json-rpc version.
void protocol_bitcoind::send_json(boost::json::value&& model,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& post = get_post();
    constexpr auto json = media_type::application_json;
    response response{ status::ok, post.version() };
    add_common_headers(response, post);
    add_access_control_headers(response, post);
    response.set(field::content_type, from_media_type(json));
    response.body() = { std::move(model), size_hint };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef SUBSCRIBE_BITCOIND
#undef CLASS

} // namespace node
} // namespace libbitcoin
