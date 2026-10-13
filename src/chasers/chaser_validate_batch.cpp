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
    // Losers commit before testing, so the retest after each drain observes
    // commits that lost the claim during it.
    while (!closed() && is_mature(residual || is_residual()))
    {
        // Admit arriving commits to the other bank (changed only under claim).
        const auto bank = bank_.load();
        bank_.store(!bank);

        // Wait for in-flight commits to the bank to complete or abandon on
        // close. Bounded: the writer epoch spans only the per-block slab
        // commit, and arriving commits go to the other bank.
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
        // ====================================================================

        const auto ec = do_process_batch(bank, false);
        if (ec == network::error::operation_canceled)
        {
            draining_.store(false);
            return;
        }

        if (ec)
        {
            draining_.store(false);
            fault(ec);
            return;
        }

        // ====================================================================

        // Log only when batch executes (non-verbose).
        log_captures();
    }

    draining_.store(false);
}

// Guarded by the drain claim (or single-threaded at startup).
code chaser_validate::do_process_batch(bool bank, bool startup) NOEXCEPT
{
    auto& query = archive();
    auto prevalids = query.get_prevalids(bank);
    const auto& cancel = stopping_;

    const auto ecdsa = query.ecdsa_records(bank);
    if (is_nonzero(ecdsa))
    {
        bool use_gpu{ gpu_ };
        header_links fails{};
        const auto start = network::logger::now();
        if (!query.verify_ecdsa_signatures(cancel, use_gpu, fails, bank))
        {
            LOGN("Batch verify ecdsa canceled (" << ecdsa << ").");
            return network::error::operation_canceled;
        }
        const auto elapsed = network::logger::now() - start;
        fire(events::ecdsa_secs, duration_cast<seconds>(elapsed).count());

        if (!startup)
        {
            LOGN(log_rate("Verify ecdsa.....", ecdsa,
                duration_cast<milliseconds>(elapsed).count(), use_gpu));
        }

        if (!mark_invalids(prevalids, fails, startup))
            return error::batch1;
    }

    const auto schnorr = query.schnorr_records(bank);
    if (is_nonzero(schnorr))
    {
        bool use_gpu{ gpu_ };
        header_links fails{};
        const auto start = network::logger::now();
        if (!query.verify_schnorr_signatures(cancel, use_gpu, fails, bank))
        {
            LOGN("Batch verify schnorr canceled (" << schnorr << ").");
            return network::error::operation_canceled;
        }
        const auto elapsed = network::logger::now() - start;
        fire(events::schnorr_secs, duration_cast<seconds>(elapsed).count());

        if (!startup)
        {
            LOGN(log_rate("Verify schnorr...", schnorr,
                duration_cast<milliseconds>(elapsed).count(), use_gpu));
        }

        if (!mark_invalids(prevalids, fails, startup))
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
        (ecdsa >= verify_target_) ||
        (schnorr >= verify_target_);
}

// Silent batch is committed while the candidate is not current.
bool chaser_validate::is_silent_capturing(const header_link& link) NOEXCEPT
{
    return silent_enabled_ && !is_current_header(link);
}

bool chaser_validate::is_silent_mature(bool residual) NOEXCEPT
{
    const auto& query = archive();
    const auto bank = silent_bank_.load();
    const auto rows = query.silent_records(bank);

    // Nothing to compute and no links to release.
    if (is_zero(rows) && is_zero(query.presilent_records(bank)))
        return false;

    // Compute residuals whenever, and non-residuals when mature.
    return residual || (rows >= silent_target_);
}

// No block is banked while both banks are empty.
bool chaser_validate::is_silent_empty() const NOEXCEPT
{
    const auto& query = archive();
    return is_zero(query.presilent_records(false)) &&
        is_zero(query.presilent_records(true));
}

std::string chaser_validate::log_rate(const std::string& name,
    size_t signatures, size_t milliseconds, bool gpu) const NOEXCEPT
{
    const auto rate = (signatures * 1000u) / greater(milliseconds, one);
    return std::format("{} ({} / {} ms) = {} sps on {}", name, signatures,
        milliseconds, rate, gpu ? "gpu" : "cpu");
}

// Silent batch.
// ----------------------------------------------------------------------------
// protected

code chaser_validate::start_silent_batch() NOEXCEPT
{
    auto& query = archive();
    for (const auto bank: { false, true })
    {
        if (is_zero(query.presilent_records(bank)) &&
            is_zero(query.silent_records(bank)))
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
    // Losers commit before testing, so the retest after each drain observes
    // commits that lost the claim during it.
    while (!closed() && is_silent_mature(residual || is_residual()))
    {
        // Admit arriving commits to the other bank (changed only under claim).
        const auto bank = silent_bank_.load();
        silent_bank_.store(!bank);

        // Wait for in-flight commits to the bank to complete or abandon on
        // close.
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
        // ====================================================================

        const auto ec = do_process_silent_batch(bank);
        if (ec == network::error::operation_canceled)
        {
            silent_draining_.store(false);
            return;
        }

        if (ec)
        {
            silent_draining_.store(false);
            fault(ec);
            return;
        }

        // ====================================================================
    }

    silent_draining_.store(false);
}

// Guarded by the drain claim (or single-threaded at startup).
code chaser_validate::do_process_silent_batch(bool bank) NOEXCEPT
{
    auto& query = archive();
    bool use_gpu{ gpu_ };
    const auto rows = query.silent_records(bank);
    const auto start = network::logger::now();
    const auto ec = query.compute_silents(stopping_, use_gpu, bank);
    if (ec == database::error::query_canceled)
    {
        LOGN("Batch silent canceled (" << rows << ").");
        return network::error::operation_canceled;
    }

    if (ec)
        return error::batch7;

    if (is_nonzero(rows))
    {
        const auto elapsed = network::logger::now() - start;
        fire(events::silent_secs, duration_cast<seconds>(elapsed).count());
        LOGN(log_rate("Compute silent...", rows,
            duration_cast<milliseconds>(elapsed).count(), use_gpu));
    }

    // Purge presilents before silents.
    if (!query.purge_presilents(bank) || !query.purge_silents(bank))
        return error::batch8;

    // Blocks of the bank may now be confirmable.
    notify(error::success, chases::bump{});
    return error::success;
}

// Upon store decline the records are set directly, within the commit epoch,
// so that a drain of the bank cannot precede them.
template <typename Block>
bool chaser_validate::commit_silent_(const header_link& link,
    const Block& block) NOEXCEPT
{
    size_t rows{};
    auto& query = archive();
    const auto bank = enter_silent_capture();
    const auto committed = query.set_silents(rows, link, block, bank);

    // Store decline (e.g. disk full), recoverable once faulted.
    if (!committed)
        fault(error::batch6);

    const auto written = committed || query.set_silent(link, block);
    exit_silent_capture(bank);
    return written;
}

bool chaser_validate::commit_silent_batch(const header_link& link,
    const chain::block& block) NOEXCEPT
{
    return commit_silent_(link, block);
}

bool chaser_validate::commit_silent_batch(const header_link& link,
    const chain::view::block& block) NOEXCEPT
{
    return commit_silent_(link, block);
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
