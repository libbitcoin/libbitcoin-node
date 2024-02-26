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
#include <bitcoin/node/chasers/chaser_header.hpp>

#include <functional>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {
    
using namespace network;
using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser(node),
    checkpoints_(config().bitcoin.checkpoints),
    currency_window_(config().node.currency_window()),
    use_currency_window_(to_bool(config().node.currency_window_minutes))
{
}

chaser_header::~chaser_header() NOEXCEPT
{
}

// protected
const network::wall_clock::duration&
chaser_header::currency_window() const NOEXCEPT
{
    return currency_window_;
}

// protected
bool chaser_header::use_currency_window() const NOEXCEPT
{
    return use_currency_window_;
}

// protected
code chaser_header::start() NOEXCEPT
{
    state_ = archive().get_candidate_chain_state(config().bitcoin);
    BC_ASSERT_MSG(state_, "Store not initialized.");

    BC_ASSERT_MSG(node_stranded(), "chaser_header");
    return subscribe(
        std::bind(&chaser_header::handle_event,
            this, _1, _2, _3));
}

// protected
void chaser_header::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_header::do_handle_event,
            this, ec, event_, value));
}

// private
void chaser_header::do_handle_event(const code&, chase event_, link) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    if (event_ == chase::stop)
        tree_.clear();
}

void chaser_header::organize(const header::cptr& header,
    result_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_header::do_organize,
            this, header, std::move(handler)));
}

// protected
// ----------------------------------------------------------------------------

void chaser_header::do_organize(const header::cptr& header_ptr,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    auto& query = archive();
    const auto& header = *header_ptr;
    const auto& previous = header.previous_block_hash();
    const auto& coin = config().bitcoin;
    const auto hash = header.hash();

    // Skip existing, fail orphan.
    // ------------------------------------------------------------------------

    if (closed())
    {
        handler(network::error::service_stopped);
        return;
    }

    // Header already exists.
    if (tree_.contains(hash) || query.is_header(hash))
    {
        handler(error::duplicate_header);
        return;
    }

    // Peer processing should have precluded orphan submission.
    if (!tree_.contains(previous) && !query.is_header(previous))
    {
        handler(error::orphan_header);
        return;
    }

    // Validate header.
    // ------------------------------------------------------------------------

    // Rolling forward chain_state eliminates requery cost.
    state_.reset(new chain_state(*state_, header, coin));
    const auto context = state_->context();
    const auto height = state_->height();

    // Checkpoints are considered chain not block/header validation.
    if (checkpoint::is_conflict(coin.checkpoints, hash, height))
    {
        handler(system::error::checkpoint_conflict);
        return;
    }

    // Header validations are not bypassed when under checkpoint/milestone.

    auto error = header.check(coin.timestamp_limit_seconds,
        coin.proof_of_work_limit, coin.scrypt_proof_of_work);
    if (error)
    {
        handler(error);
        return;
    }

    error = header.accept(context);
    if (error)
    {
        handler(error);
    }

    // Compute relative work.
    // ------------------------------------------------------------------------

    // Header is new top of stale branch (strength not computed).
    if (!is_current(header, context.height))
    {
        save(header_ptr, context);
        handler(error::success);
        return;
    }

    size_t point{};
    uint256_t work{};
    hashes tree_branch{};
    header_links store_branch{};
    if (!get_branch_work(work, point, tree_branch, store_branch, header))
    {
        handler(error::store_integrity);
        return;
    }

    bool strong{};
    if (!get_is_strong(strong, work, point))
    {
        handler(error::store_integrity);
        return;
    }

    // Header is new top of current weak branch.
    if (!strong)
    {
        save(header_ptr, context);
        handler(error::success);
        return;
    }

    // Reorganize candidate chain.
    // ------------------------------------------------------------------------

    // Obtain the top height.
    auto top = query.get_top_candidate();
    if (top < point)
    {
        handler(error::store_integrity);
        return;
    }

    // Pop down to the branch point, underflow guarded above.
    while (top-- > point)
    {
        if (!query.pop_candidate())
        {
            handler(error::store_integrity);
            return;
        }
    }

    // Push stored strong headers to candidate chain.
    for (const auto& link: views_reverse(store_branch))
    {
        if (!query.push_candidate(link))
        {
            handler(error::store_integrity);
            return;
        }
    }

    // Store strong tree headers and push to candidate chain.
    for (const auto& key: views_reverse(tree_branch))
    {
        if (!push(key))
        {
            handler(error::store_integrity);
            return;
        }
    }

    // Push new header as top of candidate chain.
    const auto link = push(header_ptr, context);
    if (link.is_terminal())
    {
        handler(error::store_integrity);
        return;
    }

    // Notify candidate reorganization with branch point.
    // ------------------------------------------------------------------------

    // New branch organized, queue up candidate downloads from branch point.
    notify(error::success, chase::header,
        { possible_narrow_cast<height_t>(point) });

    handler(error::success);
}

bool chaser_header::is_current(const header& header,
    size_t height) const NOEXCEPT
{
    if (!use_currency_window())
        return true;

    // Checkpoints are already validated. Current if at a checkpoint height.
    if (checkpoint::is_at(checkpoints_, height))
        return true;

    // en.wikipedia.org/wiki/Time_formatting_and_storage_bugs#Year_2106
    const auto time = wall_clock::from_time_t(header.timestamp());
    const auto current = wall_clock::now() - currency_window();
    return time >= current;
}

// protected
bool chaser_header::get_branch_work(uint256_t& work, size_t& point,
    hashes& tree_branch, header_links& store_branch,
    const header& header) const NOEXCEPT
{
    // Use pointer to avoid const/copy.
    auto previous = &header.previous_block_hash();
    tree_branch.clear();
    store_branch.clear();
    work = header.proof();
    const auto& query = archive();

    // Sum all branch work from tree.
    for (auto it = tree_.find(*previous); it != tree_.end();
        it = tree_.find(*previous))
    {
        previous = &it->second.header->previous_block_hash();
        tree_branch.push_back(it->second.header->hash());
        work += it->second.header->proof();
    }

    // Sum branch work from store.
    database::height_link link{};
    for (link = query.to_header(*previous); !query.is_candidate_block(link);
        link = query.to_parent(link))
    {
        uint32_t bits{};
        if (link.is_terminal() || !query.get_bits(bits, link))
            return false;

        store_branch.push_back(link);
        work += chain::header::proof(bits);
    }

    // Height of the highest candidate header is the branch point.
    return query.get_height(point, link);
}

// ****************************************************************************
// CONSENSUS: branch with greater work causes candidate reorganization.
// Chasers eventually reorganize candidate branch into confirmed if valid.
// ****************************************************************************
bool chaser_header::get_is_strong(bool& strong, const uint256_t& work,
    size_t point) const NOEXCEPT
{
    strong = false;
    uint256_t candidate_work{};
    const auto& query = archive();
    for (auto height = query.get_top_candidate(); height > point; --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_candidate(height)))
            return false;

        candidate_work += header::proof(bits);
        if (!((strong = work > candidate_work)))
            return true;
    }

    strong = true;
    return true;
}

void chaser_header::save(const header::cptr& header,
    const context& context) NOEXCEPT
{
    tree_.insert(
    {
        header->hash(),
        {
            {
                possible_narrow_cast<flags_t>(context.forks),
                possible_narrow_cast<height_t>(context.height),
                context.median_time_past,
            },
            header
        }
    });
}

database::header_link chaser_header::push(const header::cptr& header,
    const context& context) const NOEXCEPT
{
    auto& query = archive();
    const auto link = query.set_link(*header, database::context
        {
            possible_narrow_cast<flags_t>(context.forks),
            possible_narrow_cast<height_t>(context.height),
            context.median_time_past,
        });

    if (!query.push_candidate(link))
        return {};

    return link;
}

bool chaser_header::push(const system::hash_digest& key) NOEXCEPT
{
    const auto value = tree_.extract(key);
    BC_ASSERT_MSG(!value.empty(), "missing tree value");

    auto& query = archive();
    const auto& node = value.mapped();
    return query.push_candidate(query.set_link(*node.header, node.context));
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
