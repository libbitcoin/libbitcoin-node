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

// If there was a non-empty batch at startup, process it for invalids and set
// their states normally, then scan from fork point (position()) to association
// gap for all prevalids. Iterate over these setting their states to valid.
code chaser_validate::start_batch() NOEXCEPT
{
    BC_ASSERT(batched_.empty());
    if (!batch_enabled_)
        return {};

    auto& query = archive();
    batched_ = query.get_prevalids();
    if (!query.purge_prevalids())
        return error::batch1;

    if (is_zero(query.ecdsa_records()) && is_zero(query.schnorr_records()))
        return {};

    return do_process_batch(true);
}

// Shutdown drains batched_ to block prevalid states, recovered on startup.
// Snapshot restoration purges batch backlog as the tables are not append-only.
void chaser_validate::close_batch() NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT(closed());

    // Set even if signature batch tables are empty.
    if (is_zero(batch_backlog_.load()))
    {
        archive().set_prevalids(batched_);
        batched_.clear();
    }
}

// batched_ is redundant with the combined set of ecdsa/schnorr unfailed block
// identifiers stored in the two batch tables, so just a sort optimization.
void chaser_validate::push_batch(const header_link& link,
    size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Accumulate even if closed. Sacrifices stop speed to save validations.
    batched_.push_back(link);
    --batch_backlog_;

    if (closed())
    {
        close_batch();
        return;
    }

    // Unblocks check chaser for download while verifying.
    notify({}, chase::prevalid, possible_wide_cast<height_t>(height));

    // Process both tables when one hits target, allowing batched_ clearance
    // and therefore forward confirmation progress. Drain batch if no backlogs
    // and maximum hash been posted.
    const auto residual = is_residual();
    process_batch(residual);

    if (residual)
        batched_.shrink_to_fit();
}

void chaser_validate::process_batch(bool residual) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Test outside of lock to prevent reader contention for nearly all calls.
    if (closed() || !is_mature(residual))
        return;

    // Unique lock prevents batch table updates during evaluation, allowing the
    // tables to be fully purged upon completion, and ensuring that evaluation
    // does not operate over partial block records in the batch tables.
    // ========================================================================
    const std::unique_lock lock{ mutex_ };

    // Must retest inside the lock as table updates are running concurrently.
    if (closed() || !is_mature(residual))
        return;

    if (const auto ec = do_process_batch(false))
        fault(ec);
    // ========================================================================
}

code chaser_validate::do_process_batch(bool startup) NOEXCEPT
{
    if (!startup)
        log_captures();

    auto& query = archive();

    const auto ecdsa = query.ecdsa_records();
    if (is_nonzero(ecdsa))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_ecdsa_signatures(stopping_, invalids))
        {
            LOGN("Batch verify ecdsa canceled (" << ecdsa << ").");
            return network::error::operation_canceled;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::ecdsa_secs, elapsed);

        if (!startup)
        {
            LOGN(log_rate("Batch verify rate ecdsa.... ", ecdsa, elapsed));
        }

        if (!mark_invalids(invalids, startup) ||
            !query.purge_ecdsa_signatures())
            return error::batch2;
    }

    const auto schnorr = query.schnorr_records();
    if (is_nonzero(schnorr))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_schnorr_signatures(stopping_, invalids))
        {
            LOGN("Batch verify schnorr canceled (" << schnorr << ").");
            return network::error::operation_canceled;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::schnorr_secs, elapsed);

        if (!startup)
        {
            LOGN(log_rate("Batch verify rate schnorr.. ", schnorr, elapsed));
        }

        if (!mark_invalids(invalids, startup) ||
            !query.purge_schnorr_signatures())
            return error::batch3;
    }

    return mark_valids(startup) ? error::success : error::batch4;
}

// Invalids might not be included in batched, as link push is a race.
// Collected links are only required to set valid, not invalid, and do not
// need to coincide with the batch that is currently being processed (!).
// A batched_ value may not be present in the current batched_ set despite
// being invalid here, which is expected. However upon invalidation and failure
// to match here, this block id will subsequently land in batched_ and would
// then be reported as valid, overriding the unconfirmable block state set
// below, so invalids_ are cached for process lifetime.
bool chaser_validate::mark_invalids(const header_links& invalids,
    bool startup) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
            return false;

        invalids_.push_back(link);
        const auto ec = system::error::invalid_signature;
        notify_block(ec, height, link, false, startup);
    }

    // Set all invalid links in batched_ to terminal (to be skipped).
    if (!invalids_.empty())
    {
        std::ranges::replace_if(batched_, [&](const auto& link) NOEXCEPT
        {
            return contains(invalids_, link);
        }, header_link::terminal);
    }

    return true;
}

// Set all batched blocks that aren't invalid to valid.
// May be ancestors of invalid, in which case they are also unconfirmable.
bool chaser_validate::mark_valids(bool startup) NOEXCEPT
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

        notify_block(system::error::success, height, link, false, startup);
    });

    batched_.clear();
    return !fault.load();
}

// Batch helpers.
// ----------------------------------------------------------------------------
// private

bool chaser_validate::is_residual() NOEXCEPT
{
    // Verify residuals when recent.
    return maximum_posted_.load() &&
        is_zero(batch_backlog_.load()) &&
        is_zero(validate_backlog_.load());
}

bool chaser_validate::is_mature(bool residual) NOEXCEPT
{
    const auto& query = archive();
    const auto ecdsa = query.ecdsa_records();
    const auto schnorr = query.schnorr_records();

    // Verify non-residuals when mature.
    return residual || (ecdsa >= batch_target_) || (schnorr >= batch_target_);
}

std::string chaser_validate::log_rate(const std::string& name,
    size_t numerator, size_t denominator) const NOEXCEPT
{
    const auto rate = numerator / greater(denominator, one);
    return (boost_format("%1% (%2% / %3%) = %4% sps") %
        name % numerator % denominator % rate).str();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
