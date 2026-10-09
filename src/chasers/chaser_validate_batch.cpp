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
#include <format>
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
    auto& query = archive();
    for (const auto bank: { false, true })
    {
        if (is_zero(query.prevalid_records(bank)) &&
            is_zero(query.ecdsa_records(bank)) &&
            is_zero(query.schnorr_records(bank)))
            continue;

        // Prevalid is not a validation state, so dropped blocks validate in
        // place.
        if (!batch_enabled_)
        {
            LOGN("Dropping staged batch rows ("
                << query.prevalid_records(bank) << ").");

            if (const auto ec = purge_batch(bank))
                return ec;

            continue;
        }

        if (const auto ec = do_process_batch(bank, true))
            return ec;
    }

    return {};
}

void chaser_validate::process_batch(bool residual) NOEXCEPT
{
    // Cheap pre-test, retested in critical section.
    if (closed() || !is_mature(residual))
        return;

    // Claim the drain (at most one, losers rely on the winner).
    if (draining_.exchange(true))
        return;

    // Retest under the claim, another drain may have just emptied the bank.
    if (!is_mature(residual))
    {
        draining_.store(false);
        return;
    }

    // Admit arriving commits to the other bank (changed only under claim).
    const auto bank = bank_.load();
    bank_.store(!bank);

    // Wait for in-flight commits to the bank to complete or abandon on close.
    // Bounded: the writer epoch spans only the per-block slab commit, and
    // arriving commits go to the other bank.
    while (is_nonzero(writers_.at(to_int<size_t>(bank)).load()))
    {
        if (closed())
        {
            draining_.store(false);
            return;
        }

        std::this_thread::yield();
    }

    // Bank is now quiescent (no writers admitted, none in flight).
    // ========================================================================

    const auto ec = do_process_batch(bank, false);
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
code chaser_validate::do_process_batch(bool bank, bool startup) NOEXCEPT
{
    auto& query = archive();
    auto prevalids = query.get_prevalids(bank);

    const auto ecdsa = query.ecdsa_records(bank);
    if (is_nonzero(ecdsa))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_ecdsa_signatures(stopping_, invalids, bank))
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

    const auto schnorr = query.schnorr_records(bank);
    if (is_nonzero(schnorr))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_schnorr_signatures(stopping_, invalids, bank))
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

    return purge_batch(bank);
}

code chaser_validate::purge_batch(bool bank) NOEXCEPT
{
    auto& query = archive();

    // Purge prevalids before signatures.
    return query.purge_prevalids(bank) &&
        query.purge_ecdsa_signatures(bank) &&
        query.purge_schnorr_signatures(bank) ?
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
    const auto bank = bank_.load();
    const auto ecdsa = query.ecdsa_records(bank);
    const auto schnorr = query.schnorr_records(bank);

    // Nothing to verify and no links to release.
    if (is_zero(ecdsa) && is_zero(schnorr) &&
        is_zero(query.prevalid_records(bank)))
        return false;

    // Verify residuals whenever, and non-residuals when mature.
    return residual ||
        (ecdsa >= batch_target_) ||
        (schnorr >= batch_target_);
}

// Silent batch is committed while the candidate is not current.
bool chaser_validate::is_silent_capturing(const header_link& link) NOEXCEPT
{
    return batch_enabled_ && !is_current_header(link);
}

bool chaser_validate::is_silent_mature(bool residual) NOEXCEPT
{
    const auto rows = archive().silent_records(silent_bank_.load());
    return is_nonzero(rows) && (residual || rows >= batch_target_);
}

std::string chaser_validate::log_rate(const std::string& name,
    size_t signatures, size_t milliseconds) const NOEXCEPT
{
    const auto rate = (signatures * 1000u) / greater(milliseconds, one);
    return std::format("{} ({} / {} ms) = {} sps", name, signatures,
        milliseconds, rate);
}

// Silent batch.
// ----------------------------------------------------------------------------
// protected

// Silent batch is not validation state, so it drains whether or not batching
// is enabled.
code chaser_validate::start_silent_batch() NOEXCEPT
{
    for (const auto bank: { false, true })
    {
        if (is_zero(archive().silent_records(bank)))
            continue;

        if (const auto ec = do_process_silent_batch(bank))
            return ec;
    }

    return {};
}

void chaser_validate::process_silent_batch(bool residual) NOEXCEPT
{
    // Cheap pre-test, retested in critical section.
    if (closed() || !is_silent_mature(residual))
        return;

    // Claim the drain (at most one, losers rely on the winner).
    if (silent_draining_.exchange(true))
        return;

    // Retest under the claim, another drain may have just emptied the bank.
    if (!is_silent_mature(residual))
    {
        silent_draining_.store(false);
        return;
    }

    // Admit arriving commits to the other bank (changed only under claim).
    const auto bank = silent_bank_.load();
    silent_bank_.store(!bank);

    // Wait for in-flight commits to the bank to complete or abandon on close.
    while (is_nonzero(silent_writers_.at(to_int<size_t>(bank)).load()))
    {
        if (closed())
        {
            silent_draining_.store(false);
            return;
        }

        std::this_thread::yield();
    }

    // Bank is now quiescent (no writers admitted, none in flight).
    // ========================================================================

    const auto ec = do_process_silent_batch(bank);
    silent_draining_.store(false);
    if (ec == network::error::operation_canceled)
        return;

    if (ec)
        fault(ec);

    // ========================================================================
}

// Guarded by the drain claim (or single-threaded at startup).
code chaser_validate::do_process_silent_batch(bool bank) NOEXCEPT
{
    auto& query = archive();
    const auto rows = query.silent_records(bank);
    const auto start = network::logger::now();
    const auto ec = query.compute_silents(stopping_, bank);
    if (ec == database::error::query_canceled)
    {
        LOGN("Batch silent canceled (" << rows << ").");
        return network::error::operation_canceled;
    }

    if (ec)
        return error::batch7;

    const auto elapsed = network::logger::now() - start;
    LOGN(log_rate("Compute silent...", rows,
        duration_cast<milliseconds>(elapsed).count()));

    if (!query.purge_silents(bank))
        return error::batch8;

    // Blocks of the bank may now be confirmable.
    silent_heights_.at(to_int<size_t>(bank)).store(max_size_t);
    notify(error::success, chases::bump{});
    return error::success;
}

// The bank height is lowered before exit, so that its drain observes it.
bool chaser_validate::commit_silent_batch(const header_link& link,
    const chain::block& block, size_t height) NOEXCEPT
{
    size_t rows{};
    const auto bank = enter_silent_capture();
    const auto committed = archive().set_silents(rows, link, block, bank);
    if (committed && is_nonzero(rows))
    {
        auto& lowest = silent_heights_.at(to_int<size_t>(bank));
        auto current = lowest.load();
        while (height < current)
            if (lowest.compare_exchange_weak(current, height))
                break;
    }

    exit_silent_capture(bank);

    // Store decline (e.g. disk full), recoverable once faulted.
    if (!committed)
        fault(error::batch6);

    return committed;
}

size_t chaser_validate::silent_limit() const NOEXCEPT
{
    return std::min(silent_heights_.front().load(),
        silent_heights_.back().load());
}

// A writer admitted to a bank that is then switched retries on the other, so
// that a drain observes every writer of its bank.
bool chaser_validate::enter_silent_capture() NOEXCEPT
{
    while (true)
    {
        const auto bank = silent_bank_.load();
        ++silent_writers_.at(to_int<size_t>(bank));
        if (bank == silent_bank_.load())
            return bank;

        --silent_writers_.at(to_int<size_t>(bank));
    }
}

void chaser_validate::exit_silent_capture(bool bank) NOEXCEPT
{
    --silent_writers_.at(to_int<size_t>(bank));
}

// Turnstile.
// ----------------------------------------------------------------------------
// protected

// A writer admitted to a bank that is then switched retries on the other, so
// that a drain observes every writer of its bank.
bool chaser_validate::enter_capture() NOEXCEPT
{
    while (true)
    {
        const auto bank = bank_.load();
        ++writers_.at(to_int<size_t>(bank));
        if (bank == bank_.load())
            return bank;

        --writers_.at(to_int<size_t>(bank));
    }
}

void chaser_validate::exit_capture(bool bank) NOEXCEPT
{
    --writers_.at(to_int<size_t>(bank));
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
