/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_OUTBOUND_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_OUTBOUND_HPP

#include <unordered_map>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API session_outbound
  : public attach<network::session_outbound>
{
public:
    session_outbound(full_node& node, uint64_t identifier) NOEXCEPT;

    void performance(uint64_t channel, size_t speed,
        network::result_handler&& handler) NOEXCEPT override;

protected:
    void attach_protocols(
        const network::channel::ptr& channel) NOEXCEPT override;

private:
    void do_performance(uint64_t channel, size_t speed,
        const network::result_handler& handler) NOEXCEPT;

    // This is protected by strand.
    std::unordered_map<uint64_t, double> speeds_;
};

} // namespace node
} // namespace libbitcoin

#endif
