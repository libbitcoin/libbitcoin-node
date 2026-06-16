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

#include <ranges>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::chain;
using namespace database;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// TODO: ecdsa can be retained, as they don't fault, so set batched_ here.
// Cannot know if archived batch is faulted, despite being otherwise full, as
// faulted is a non-persistent state. So we must purge batches at start.
code chaser_validate::start_batch() NOEXCEPT
{
    auto& query = archive();
    return (batch_signatures_ && (!query.purge_ecdsa_signatures() ||
        !query.purge_schnorr_signatures())) ? error::batch1 : error::success;
}

void chaser_validate::push_batch(const header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());
    batched_.push_back(link);
}

// TODO: This is only invoked by check chaser advancement. But it is possible
// that entries may be captured after that point. So this must be bumped.
void chaser_validate::process_batch() NOEXCEPT
{
    BC_ASSERT(stranded());

    // Unique lock prevents batch table updates during evaluation, allowing the
    // tables to be fully purged upon completion, and ensuring that evaluation
    // does not operate over partial block records in the batch tables.
    std::unique_lock lock(mutex_);

    auto& query = archive();
    LOGN("Batch signature verify begin ("
        << query.ecdsa_records() << ") ecdsa ("
        << query.schnorr_records() << ") schnorr.");

    // set_block_unconfirmable
    // ------------------------------------------------------------------------

    header_links invalids{};
    auto start = network::logger::now();
    if (!query.verify_ecdsa_signatures(invalids))
    {
        fault(error::batch2);
        return;
    }
    span<milliseconds>(events::ecdsa_msecs, start);

    if (!process_invalids(invalids, "ecdsa") ||
        !query.purge_ecdsa_signatures())
    {
        fault(error::batch3);
        return;
    }

    invalids.clear();
    start = network::logger::now();
    if (!query.verify_schnorr_signatures(invalids))
    {
        fault(error::batch4);
        return;
    }
    span<milliseconds>(events::schnorr_msecs, start);

    if (!process_invalids(invalids, "schnorr") ||
        !query.purge_schnorr_signatures())
    {
        fault(error::batch5);
        return;
    }

    // set_block_valid
    // ------------------------------------------------------------------------

    if (!process_valids())
    {
        fault(error::batch6);
        return;
    }

    // ------------------------------------------------------------------------

    LOGN("Batch signature verify end.");
}

// Invalids might not be included in batched, as link push is a race.
// Collected links are only required to set valid, not invalid, and do not
// need to coincide with the batch that is currently being processed (!).
bool chaser_validate::process_invalids(const header_links& invalids,
    const std::string_view& name) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
            return false;

        LOGR("Invalid " << name << " signature in block (" << height << ").");
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
bool chaser_validate::process_valids() NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: batched_)
    {
        // Terminal links are previously set invalid (to be skipped).
        if (link == header_link::terminal)
            continue;

        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_valid(link))
            return false;

        notify_block(system::error::block_success, height, link, false);
    }

    batched_.clear();
    if (is_current(true))
        batched_.shrink_to_fit();

    return true;
}

signatures chaser_validate::get_capture(const header_link& link) NOEXCEPT
{
    if (!batch_signatures_ || is_current(link))
        return { false };

    // This call is blocked during signature batch evaluation and all
    // outstanding captures block signature batch evaluation until complete.
    const auto lock = emplace_shared<shared_lock>(mutex_);

    const auto sequence = to_shared<atomic_counter>();
    return signatures
    {
        .enabled = true,
        .log = BIND_THIS(do_log, _1),
        .fire = BIND_THIS(do_fire, _1, _2, lock),
        .ecdsa = BIND_THIS(do_ecdsa, _1, _2, _3, link),
        .schnorr = BIND_THIS(do_schnorr, _1, _2, _3, link),
        .multisig = BIND_THIS(do_multisig, _1, _2, _3, link, sequence),
        .threshold = BIND_THIS(do_threshold, _1, link, sequence)
    };
}

// private
// ----------------------------------------------------------------------------

void chaser_validate::do_log(const script& ) NOEXCEPT
{
    // Enable for a game of whack-a-mole.
    ////LOGA("Sigop @ " << ctx.height << " -> "
    ////    << missed.to_string(flags::all_rules));
}

// Captures shared lock on batch verification.
void chaser_validate::do_fire(missed miss, size_t count,
    const shared_lock_cptr&) NOEXCEPT
{
    switch (miss)
    {
        case missed::ecdsa:
            missed_ecdsa_ += count;
            break;
        case missed::multisig:
            missed_multisig_ += count;
            break;
        case missed::schnorr:
            missed_schnorr_ += count;
            break;
        default:;
    }
}

bool chaser_validate::do_ecdsa(const hash_digest& digest,
    const ec_compressed& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++ecdsa_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch7);
    return set;
}

bool chaser_validate::do_schnorr(const hash_digest& digest,
    const ec_xonly& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++schnorr_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch8);
    return set;
}

bool chaser_validate::do_multisig(const hash_digest& digest,
    const ec_compresseds& points, const ec_signatures& signs,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    BC_ASSERT(points.size() == signs.size());

    multisig_ += points.size();
    ////const auto set = archive().set_signatures(digest, points, signs,
    ////    (*sequence)++, link);
    ////if (!set) fault(error::batch9);
    ////return set;
    return true;
}

bool chaser_validate::do_threshold(const threshold_group& group,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    threshold_ += group.entries.size();
    ////const auto set = archive().set_signatures(group, (*sequence)++, link);
    ////if (!set) fault(error::batch10);
    ////return set;
    return true;
}

void chaser_validate::log_capture(const std::string_view& name,
    size_t captured, size_t missed) const NOEXCEPT
{
    if (to_bool(captured) || to_bool(missed))
    {
        const auto rate = (100.0f * captured) / (captured + missed);
        const auto text = (boost_format("%.4f") % rate).str();
        LOGV("Capture rate " << name << text << "% = " << captured
            << "/(" << captured << "+" << missed << ")");
    }
}

void chaser_validate::log_captures() const NOEXCEPT
{
    log_capture("ecdsa.... ", ecdsa_, missed_ecdsa_);
    log_capture("multisig. ", multisig_, missed_multisig_);
    log_capture("schnorr.. ", schnorr_, missed_schnorr_);
    log_capture("threshold ", threshold_, zero);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
