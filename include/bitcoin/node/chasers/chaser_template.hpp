/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP

#include <atomic>
#include <vector>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Hold the template of the next block, replaced as chain and pool move.
class BCN_API chaser_template
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_template);

    chaser_template(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

    /// The template held, built first if the chain or pool has moved.
    virtual void get_template(block_template_handler&& handler) NOEXCEPT;

    /// Count a demand for template replacement by interval (or uncount).
    virtual void demand_template(bool demand) NOEXCEPT;

    /// Sequence of the template held, zero if none yet (thread safe).
    virtual uint64_t template_generation() const NOEXCEPT;

protected:
    virtual bool handle_chase(const code& ec, event_value value) NOEXCEPT;

    virtual void do_stopping(const code& ec) NOEXCEPT;
    virtual void do_get_template(
        const block_template_handler& handler) NOEXCEPT;
    virtual void do_demand_template(bool demand) NOEXCEPT;

    /// The chain has moved, so the template held is not of its top.
    virtual void do_moved(bool settled) NOEXCEPT;

private:
    void build() NOEXCEPT;
    void handle_build(const code& ec, size_t height,
        const database::tx_links& links, uint64_t generation,
        size_t moves) NOEXCEPT;
    void do_build(const code& ec, size_t height,
        const database::tx_links& links, uint64_t generation,
        size_t moves) NOEXCEPT;
    code replace(size_t height, const database::tx_links& links) NOEXCEPT;
    void complete(const code& ec) NOEXCEPT;
    void start_timer() NOEXCEPT;
    void handle_timer(const code& ec) NOEXCEPT;

    // This is thread safe.
    std::atomic<uint64_t> generation_{};

    // These are protected by strand.
    block_template::cptr held_{};
    std::vector<block_template_handler> waiting_{};
    network::deadline::ptr timer_{};
    uint64_t built_{};
    size_t moves_{};
    size_t moved_{};
    size_t demand_{};
    bool building_{};
};

} // namespace node
} // namespace libbitcoin

#endif
