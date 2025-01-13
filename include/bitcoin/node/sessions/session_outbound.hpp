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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_OUTBOUND_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_OUTBOUND_HPP

#include <unordered_map>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/attach.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API session_outbound
  : public attach<network::session_outbound>,
    protected network::tracker<session_outbound>
{
public:
    typedef std::shared_ptr<session_outbound> ptr;

    session_outbound(full_node& node, uint64_t identifier) NOEXCEPT;

    void start(network::result_handler&& handler) NOEXCEPT override;
    void performance(object_key channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT override;

protected:
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;
    virtual void do_starved(object_t self) NOEXCEPT;
    virtual void do_performance(object_key channel, uint64_t speed,
        const network::result_handler& handler) NOEXCEPT;

private:
    static constexpr size_t minimum_for_standard_deviation = 3;

    // This is thread safe.
    const float allowed_deviation_;

    // This is protected by strand.
    // TODO: optimize, default bucket count is around 8.
    std::unordered_map<object_key, double> speeds_{};
};

} // namespace node
} // namespace libbitcoin

#endif
