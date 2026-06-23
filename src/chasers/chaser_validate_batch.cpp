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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <atomic>
#include <algorithm>
#include <mutex>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace database;
using namespace std::chrono;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Batching.
// ----------------------------------------------------------------------------
// protected

// TODO: ecdsa can be retained, as they don't fault, so set batched_ here.
// Cannot know if archived batch is faulted, despite being otherwise full, as
// faulted is a non-persistent state. So we must purge batches at start.
code chaser_validate::start_batch() NOEXCEPT
{
    auto& query = archive();
    return (batch_enabled_ &&
        (!query.purge_ecdsa_signatures() ||
         !query.purge_schnorr_signatures())) ?
        error::batch1 : error::success;
}

// batched_ is redundant with the combined set of ecdsa/schnorr unfailed block
// identifiers stored in the two batch tables, so just a sort optimization.
void chaser_validate::push_batch(const header_link& link, size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed()) return;
    batched_.push_back(link);
    --batch_backlog_;

    // Unblocks check chaser.
    notify({}, chase::prevalid, possible_wide_cast<height_t>(height));

    // Process both tables when one hits target, allowing batched_ clearance
    // and therefore forward confirmation progress. Drain batch if no backlogs
    // and maximum hash been posted.
    process_batch(is_maximum());
}

void chaser_validate::process_batch(bool residual) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Test outside of lock to prevent reader contention for nearly all calls.
    auto& query = archive();
    if (closed() || (!residual &&
        (query.ecdsa_records() < batch_target_) &&
        (query.schnorr_records() < batch_target_)))
        return;

    // Unique lock prevents batch table updates during evaluation, allowing the
    // tables to be fully purged upon completion, and ensuring that evaluation
    // does not operate over partial block records in the batch tables.
    // ========================================================================
    const std::unique_lock lock{ mutex_ };

    // Must retest inside the lock as table updates are running concurrently.
    if (closed()) return;
    const auto ecdsa = query.ecdsa_records();
    const auto schnorr = query.schnorr_records();
    if (!residual && (ecdsa < batch_target_) && (schnorr < batch_target_))
        return;

    log_captures();

    // set_block_unconfirmable(ecdsa)
    // ------------------------------------------------------------------------

    if (is_nonzero(ecdsa))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_ecdsa_signatures(stopping_, invalids))
        {
            LOGN("Batch verify ecdsa canceled (" << ecdsa << ").");
            return;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::ecdsa_secs, elapsed);
        LOGN(log_rate("Batch verify rate ecdsa.... ", ecdsa, elapsed));

        if (!process_invalids(invalids) || !query.purge_ecdsa_signatures())
        {
            fault(error::batch2);
            return;
        }
    }

    // set_block_unconfirmable(schnorr)
    // ------------------------------------------------------------------------

    if (is_nonzero(schnorr))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_schnorr_signatures(stopping_, invalids))
        {
            LOGN("Batch verify schnorr canceled (" << schnorr << ").");
            return;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::schnorr_secs, elapsed);
        LOGN(log_rate("Batch verify rate schnorr.. ", schnorr, elapsed));

        if (!process_invalids(invalids) || !query.purge_schnorr_signatures())
        {
            fault(error::batch3);
            return;
        }
    }

    // set_block_valid(batched_ excluding ecdsa/schnorr failures)
    // ------------------------------------------------------------------------

    if (!process_valids(residual))
    {
        fault(error::batch4);
        return;
    }
    // ========================================================================
}

// Batching helpers.
// ----------------------------------------------------------------------------
// private

bool chaser_validate::is_maximum() NOEXCEPT
{
    return maximum_posted_.load() &&
        is_zero(batch_backlog_.load()) &&
        is_zero(validate_backlog_.load());
}

// Invalids might not be included in batched, as link push is a race.
// Collected links are only required to set valid, not invalid, and do not
// need to coincide with the batch that is currently being processed (!).
bool chaser_validate::process_invalids(const header_links& invalids) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
            return false;

        notify_block(system::error::invalid_signature, height, link, false);
    }

    // Set all invalids links in batched_ to terminal (to be skipped).
    if (!invalids.empty())
    {
        std::ranges::replace_if(batched_, [&](const auto& link) NOEXCEPT
        {
            return contains(invalids, link);
        }, header_link::terminal);
    }

    return true;
}

// Set all batched blocks that aren't invalid to valid.
// May be ancestors of invalid, in which case they are also unconfirmable.
bool chaser_validate::process_valids(bool residual) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    std::atomic_bool fault{};
    constexpr auto parallel = poolstl::execution::par;

    std::for_each(parallel, batched_.cbegin(), batched_.cend(),
        [&](auto link) NOEXCEPT
    {
        // terminal links are previously set invalid (to be skipped).
        if (link == header_link::terminal)
            return;

        size_t height{};
        if (!query.get_height(height, link) || !query.set_block_valid(link))
        {
            fault.store(true);
            return;
        }

        notify_block(system::error::success, height, link, false);
    });

    batched_.clear();
    if (residual)
        batched_.shrink_to_fit();

    return fault.load();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
