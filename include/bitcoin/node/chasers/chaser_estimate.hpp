/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_ESTIMATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_ESTIMATE_HPP

#include <atomic>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/estimator.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Maintain a running fee estimate cache.
class BCN_API chaser_estimate
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_estimate);

    chaser_estimate(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

    /// Returns max_uint64 when disabled.
    void estimate(size_t target, estimator::mode mode,
        estimate_handler&& handler) NOEXCEPT;

    /// Returns zero when disabled (thread safe).
    size_t top_height() const NOEXCEPT;

    /// Initialization is complete and successful.
    bool initialized() const NOEXCEPT;

protected:
    virtual bool handle_chase(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_initialize(header_t value) NOEXCEPT;
    virtual void do_organized(header_t value) NOEXCEPT;
    virtual void do_reorganized(header_t value) NOEXCEPT;

private:
    void do_estimate(size_t target, estimator::mode mode,
        const estimate_handler& handler) NOEXCEPT;

    // These are thread safe.
    std::atomic_bool stopping_{};
    std::atomic_bool initialized_{};

    // This is protected by strand.
    estimator::ptr estimator_{};
};

} // namespace node
} // namespace libbitcoin

#endif
