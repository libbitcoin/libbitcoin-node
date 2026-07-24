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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <atomic>
#include <algorithm>
#include <thread>
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

code chaser_validate::start_batch() NOEXCEPT
{
    if (!batch_enabled_)
        return {};

    const auto& query = archive();
    if (is_zero(query.prevalid_records()) &&
        is_zero(query.ecdsa_records()) &&
        is_zero(query.schnorr_records()))
        return {};

    return do_process_batch(true);
}

void chaser_validate::process_batch(bool residual) NOEXCEPT
{
    // Cheap pre-test, retested in critical section.
    if (closed() || !is_mature(residual))
        return;

    // Claim the drain (at most one, losers rely on the winner).
    if (draining_.exchange(true))
        return;

    // Wait for in-flight commits to complete or abandon on close.
    // Bounded: the writer epoch spans only the per-block slab commit, so
    // writers_ only drains (arriving commits divert to in-place verify).
    while (is_nonzero(writers_.load()))
    {
        if (closed())
        {
            draining_.store(false);
            return;
        }

        std::this_thread::yield();
    }

    // Batch tables are now quiescent (no writers admitted, none in flight).
    // ========================================================================

    // Retest under the claim, another drain may have just emptied the tables.
    if (!is_mature(residual))
    {
        draining_.store(false);
        return;
    }

    const auto ec = do_process_batch(false);
    draining_.store(false);
    if (ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        fault(ec);
        return;
    }

    // ========================================================================

    // Log outside of drain claim, and only when batch executes (non-verbose).
    log_captures();
}

// Guarded by the drain claim (or single-threaded at startup).
code chaser_validate::do_process_batch(bool startup) NOEXCEPT
{
    auto& query = archive();
    auto prevalids = query.get_prevalids();

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
        const auto elapsed = network::logger::now() - start;
        fire(events::ecdsa_secs, duration_cast<seconds>(elapsed).count());

        if (!startup)
        {
            LOGN(log_rate("Verify ecdsa.....", ecdsa,
                duration_cast<milliseconds>(elapsed).count()));
        }

        if (!mark_invalids(prevalids, invalids, startup))
            return error::batch1;
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
        const auto elapsed = network::logger::now() - start;
        fire(events::schnorr_secs, duration_cast<seconds>(elapsed).count());

        if (!startup)
        {
            LOGN(log_rate("Verify schnorr...", schnorr,
                duration_cast<milliseconds>(elapsed).count()));
        }

        if (!mark_invalids(prevalids, invalids, startup))
            return error::batch2;
    }

    if (!mark_valids(prevalids, startup))
        return error::batch3;

    // Purge prevalids before signatures.
    return query.purge_prevalids() &&
        query.purge_ecdsa_signatures() &&
        query.purge_schnorr_signatures() ?
        error::success : error::batch4;
}

bool chaser_validate::mark_invalids(header_links& prevalids,
    const header_links& invalids, bool startup) NOEXCEPT
{
    auto& query = archive();
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
            return false;

        const auto ec = system::error::invalid_signature;
        notify_block(ec, height, link, false, startup);
    }

    // Exclude invalid links from marking, invalids is almost always empty.
    if (!invalids.empty())
    {
        std::ranges::replace_if(prevalids, [&](const header_link& link) NOEXCEPT
        {
            return contains(invalids, link);
        }, header_link::terminal);
    }

    return true;
}

// Set all prevalid blocks that aren't invalid to valid.
// May be ancestors of invalid, in which case they are also unconfirmable.
bool chaser_validate::mark_valids(header_links& prevalids,
    bool startup) NOEXCEPT
{
    auto& query = archive();
    std::atomic_bool fault{};
    constexpr auto parallel = poolstl::execution::par;

    // Allow valids to drain when closed.
    std::for_each(parallel, prevalids.cbegin(), prevalids.cend(),
        [&](auto link) NOEXCEPT
    {
        // Terminal links are those flagged as invalid (to be skipped).
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

    return !fault.load();
}

// Batch helpers.
// ----------------------------------------------------------------------------
// private

bool chaser_validate::is_residual() NOEXCEPT
{
    // Verify residuals when recent or window is fully archived.
    return (maximum_posted_.load() || window_archived_.load()) &&
        is_zero(validate_backlog_.load());
}

bool chaser_validate::is_mature(bool residual) NOEXCEPT
{
    const auto& query = archive();
    const auto ecdsa = query.ecdsa_records();
    const auto schnorr = query.schnorr_records();

    // Nothing to verify and no links to release.
    if (is_zero(ecdsa) && is_zero(schnorr) &&
        is_zero(query.prevalid_records()))
        return false;

    // Verify residuals whenever, and non-residuals when mature.
    return residual ||
        (ecdsa >= batch_target_) ||
        (schnorr >= batch_target_);
}

std::string chaser_validate::log_rate(const std::string& name,
    size_t signatures, size_t milliseconds) const NOEXCEPT
{
    const auto rate = (signatures * 1000u) / greater(milliseconds, one);
    return (boost_format("%1% (%2% / %3% ms) = %4% sps") %
        name % signatures % (milliseconds) % rate).str();
}

// Turnstile.
// ----------------------------------------------------------------------------
// protected

bool chaser_validate::enter_capture() NOEXCEPT
{

    // Capture bypassed by batch.
    ++writers_;
    if (draining_.load())
    {
        --writers_;
        return false;
    }

    return true;
}

void chaser_validate::exit_capture() NOEXCEPT
{
    --writers_;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
