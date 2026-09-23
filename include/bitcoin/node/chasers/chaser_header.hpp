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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_HEADER_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_HEADER_HPP

#include <unordered_map>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger header branches for the candidate chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'header' event.
/// Headers are validated and proven by the protocol, so all are storable.
class BCN_API chaser_header
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_header);

    chaser_header(full_node& node) NOEXCEPT;

    /// Validate a header against its chain state.
    static code validate(const system::chain::header& header,
        const system::chain::chain_state& state,
        const system::settings& settings) NOEXCEPT;

    /// Initialize chaser state.
    code start() NOEXCEPT override;

    /// Validate and organize an unproven header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a proven header, milestone set if in milestone branch.
    virtual void organize(const system::chain::header::cptr& header,
        bool milestone, organize_handler&& handler) NOEXCEPT;

    /// Reorganize to the branch of an archived block of at least equal work.
    virtual void prioritize(const system::hash_digest& hash,
        organize_handler&& handler) NOEXCEPT;

    /// Work a proven header branch must reach (candidate or configured).
    virtual void get_minimum_work(work_handler&& handler) NOEXCEPT;

protected:
    using header_link = database::header_link;
    using chain_state = system::chain::chain_state;

    using header_tree = std::unordered_map<system::hash_cref,
        system::chain::header::cptr>;

    /// Handle chaser events.
    virtual bool handle_chase(const code&, event_value value) NOEXCEPT;

    /// Organize a discovered header, prioritized accepts a tied branch.
    virtual void do_organize(const system::chain::header::cptr& header,
        bool prioritized, bool milestone, bool proven,
        const organize_handler& handler) NOEXCEPT;

    /// Reorganize following block unconfirmability.
    virtual void do_disorganize(header_t header) NOEXCEPT;

    /// Reorganize to the branch of the given block.
    virtual void do_prioritize(const system::hash_digest& hash,
        const organize_handler& handler) NOEXCEPT;

    /// Obtain the work a proven header branch must reach.
    virtual void do_get_minimum_work(const work_handler& handler) NOEXCEPT;

private:
    using header_links = database::header_links;
    using header_states = database::header_states;

    // Validation.
    code duplicate(size_t& height,
        const system::hash_digest& hash) const NOEXCEPT;

    // Setters.
    bool set_reorganized(height_t candidate_height) NOEXCEPT;
    bool set_organized(const header_link& link,
        height_t candidate_height) NOEXCEPT;
    code push_header(const system::hash_digest& key) NOEXCEPT;
    code push_header(const system::chain::header& header,
        const system::chain::context& ctx, bool milestone) NOEXCEPT;
    void cache(const system::chain::header::cptr& header,
        const chain_state::cptr& state) NOEXCEPT;

    // Checkpoint gate.
    bool is_under_active_checkpoint(
        const system::hash_digest& previous) const NOEXCEPT;
    void update_checkpoint(height_t top) NOEXCEPT;

    // Tree control.
    void shrink_tree(bool current) NOEXCEPT;
    void prune_tree(const uint256_t& threshold) NOEXCEPT;
    void prune_tree(height_t top) NOEXCEPT;
    void prune_tree() NOEXCEPT;

    // Getters.
    size_t get_window() const NOEXCEPT;
    uint256_t get_window_work() const NOEXCEPT;
    chain_state::cptr get_chain_state(
        const system::hash_digest& previous_hash) const NOEXCEPT;
    bool get_branch_work(uint256_t& branch_work,
        system::hashes& tree_branch, header_states& store_branch,
        const system::chain::header& header) const NOEXCEPT;

    // Logging.
    void log_state_change(const chain_state& from,
        const chain_state& to) const NOEXCEPT;

    // These are thread safe.
    const system::settings& settings_;
    const system::chain::checkpoints& checkpoints_;

    // These are protected by strand.
    bool bumped_{};
    bool shrunk_{};
    size_t next_window_{};
    size_t next_checkpoint_{};
    size_t active_checkpoint_{};
    chain_state::cptr state_{};
    header_tree tree_{};
};

} // namespace node
} // namespace libbitcoin

#endif
