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
#include <ranges>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Independent threadpool and strand (base class strand uses network pool).
chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    validation_threadpool_(node.node_settings().threads_(),
        node.node_settings().thread_priority_()),
    validation_strand_(validation_threadpool_.service().get_executor()),
    subsidy_interval_(node.system_settings().subsidy_interval_blocks),
    initial_subsidy_(node.system_settings().initial_subsidy()),
    maximum_backlog_(node.node_settings().maximum_concurrency_()),
    batch_signatures_(node.node_settings().batch_signatures),
    node_witness_(node.network_settings().witness_node()),
    filter_(node.archive().filter_enabled())
{
}

code chaser_validate::start() NOEXCEPT
{
    if (!node_settings().headers_first)
        return error::success;

    // TODO: ecdsa can be retained, as they don't fault, so set batched_ here.
    // Cannot know if archived batch is faulted, despite being otherwise full,
    // as faulted is a non-persistent state. So we must purge batches at start.
    auto& query = archive();
    if (batch_signatures_ && !query.purge_signatures())
        return error::batch1;

    set_position(query.get_fork());
    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    // Incoming events may already be flushed to the strand at this point.
    if (suspended())
        return true;

    switch (event_)
    {
        case chase::resume:
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_checked, std::get<height_t>(value));
            break;
        }
        case chase::advanced:
        {
            if (!batch_signatures_)
                break;

            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_advanced, std::get<height_t>(value));
            break;
        }
        case chase::regressed:
        case chase::disorganized:
        {
            // value is regression branch_point.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_regressed, std::get<height_t>(value));
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Track downloaded
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (branch_point >= position())
        return;

    set_position(branch_point);
}

void chaser_validate::do_advanced(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    process_batch();
}

void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Cannot validate next block until all previous blocks are archived.
    if (height == add1(position()))
        do_bumped(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto height = add1(position());
    if (archive().is_validateable(height))
        do_bumped(height);
}

// Validate (cancellable)
// ----------------------------------------------------------------------------

void chaser_validate::do_bumped(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Bypass until next event if validation backlog is full.
    // Stop when suspended as write error does not terminate asynchronous loop.
    while ((backlog_ < maximum_backlog_) && !closed() && !suspended())
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Must exit on unassociated so they are not set valid in bypass.
        // Given height-based iteration, any block state may be enountered.
        if (ec == database::error::unassociated)
            return;

        const auto bypass = is_under_checkpoint(height) ||
            query.is_milestone(link);

        switch (ec.value())
        {
            case database::error::unvalidated:
            case database::error::unknown_state:
            {
                if (!bypass || filter_)
                    post_block(link, bypass);
                else
                    complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_valid:
            case database::error::block_confirmable:
            {
                complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_unconfirmable:
            {
                return;
            }
            ////case database::error::unassociated
            default:
            {
                fault(error::validate1);
                return;
            }
        }

        // All posted validations must complete or this is invalid.
        // So posted validations continue despite network suspension.
        set_position(height++);
    }
}

void chaser_validate::post_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    BC_ASSERT(stranded());

    backlog_.fetch_add(one, relaxed);
    PARALLEL(validate_block, link, bypass);
}

// Unstranded (concurrent by block)
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    bool batched{}, faulted{};
    auto& query = archive();

    // TODO: implement allocator parameter resulting in full allocation to
    // shared_ptr<block>, to optimize deallocate (12% of milestone/filter).
    const auto block = query.get_block(link, node_witness_);

    if (!block)
    {
        ec = error::validate2;
    }
    else if (!query.get_context(ctx, link))
    {
        ec = error::validate3;
    }
    else if ((ec = populate(bypass, *block, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate4;
    }
    else if ((ec = validate(batched, faulted, bypass, *block, link, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate5;
    }

    complete_block(ec, link, ctx.height, bypass, batched, faulted);

    // Prevent stall by posting internal event, avoiding external handlers.
    if (is_one(backlog_.fetch_sub(one, relaxed)))
        handle_chase({}, chase::bump, height_t{});
}

code chaser_validate::populate(bool bypass, const chain::block& block,
    const chain::context& ctx) NOEXCEPT
{
    const auto& query = archive();

    if (bypass)
    {
        // Populating for filters only (no validation metadata required).
        block.populate(ctx);
        if (!query.populate_without_metadata(block))
            return system::error::missing_previous_output;
    }
    else
    {
        // Internal maturity and time locks are verified here because they are
        // the only necessary confirmation checks for internal spends.
        if (const auto ec = block.populate(ctx))
            return ec;

        // Metadata identifies internal spends allowing confirmation bypass.
        if (!query.populate_with_metadata(block))
            return system::error::missing_previous_output;
    }
    
    return error::success;
}

code chaser_validate::validate(bool& batched, bool& faulted, bool bypass,
    const chain::block& block, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();

    if (!bypass)
    {
        code ec{};
        if (((ec = block.check(false))) || ((ec = block.check(ctx, false))))
            return ec;

        if ((ec = block.accept(ctx, subsidy_interval_, initial_subsidy_)))
            return ec;

        // Initialize block capture.
        const auto capture = get_capture(link);

        // Sequentially connect block with signature capture (if enabled).
        // There is not stop during connect, so a shutdown will wait on the
        // completion (block consistency) of all signature captures. However
        // the faulted state of a batch is not persisted (because disk full).
        if ((ec = block.connect(ctx, capture)))
            return ec;

        // At least one signature batch was attempted (defer completion).
        batched = capture.batched;

        // Threshold batch commit failed, block otherwise passed (retry block).
        faulted = capture.faulted;

        // Prevouts optimize confirmation.
        // Block will be retried if batch is faulted.
        if (!faulted && !query.set_prevouts(link, block))
            return error::validate6;
    }

    // Block will be retried if batch is faulted.
    if (!faulted && !query.set_filter_body(link, block))
        return error::validate7;

    // Defer block state change when batched (or faulted).
    // Valid must be set after set_prevouts and set_filter_body.
    if (!batched && !bypass && !query.set_block_valid(link))
        return error::validate8;

    return error::success;
}

// May be either concurrent or stranded.
void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height, bool bypass, bool batched, bool faulted) NOEXCEPT
{
    // Node errors are fatal (or disk full recoverable).
    if (ec && node::error::error_category::contains(ec))
    {
        fault(ec);
        return;
    }

    // Prioritize non-signature block validation failures.
    if (ec)
    {
        notify_block(ec, height, link, bypass);
        return;
    }

    // At least one unrecoverable (threshold) capture failed during script
    // validations, and there was no other failure. This is only caused by a
    // store fault - possibly a disk full condition. In the case of disk full
    // the node will pause, otherwise it will halt. Assume disk full here,
    // requiring a repost for block validation.
    if (faulted)
    {
        POST(post_block, link, bypass);
        return;
    }

    // Push block link to batched_, process_batch will verify via batch.
    // If block is missed it will be picked up on next batch, or on restart.
    if (batched)
    {
        POST(push_batch, link);
        return;
    }

    // Not failed/invalid/batched/faulted, so block is complete (maybe valid).
    notify_block({}, height, link, bypass);
}

void chaser_validate::push_batch(const header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());
    batched_.push_back(link);
}

// TODO: This is only invoked by check chaser advancement. But it's possible
// entries may be captured after that point. So this must be bumped.
void chaser_validate::process_batch() NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Unique lock prevents batch table updates during evaluation, allowing the
    // tables to be fully purged upon completion, and ensuring that evaluation
    // does not operate over partial block records in the batch tables.
    std::unique_lock lock(mutex_);

    LOGN("Batch verification begin.");

    header_links invalids{};
    if (!query.verify_signatures(invalids))
    {
        fault(error::batch2);
        return;
    }

    // Invalids might not be included in batched, as link push is a race.
    // Collected links are only required to set valid, not invalid, and do not
    // need to coincide with the batch that is currently being processed (!).
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
        {
            fault(error::batch3);
            return;
        }

        notify_block(system::error::invalid_signature, height, link, false);
    }

    // Set all invalid links in batched_ to terminal.
    std::ranges::replace_if(batched_, [&](const auto& link) NOEXCEPT
    {
        return contains(invalids, link);
    }, header_link::terminal);

    // Set all batched blocks that aren't invalid to valid.
    // May be ancestors of invalid, in which case they are also unconfirmable.
    for (const auto& link: batched_)
    {
        // Terminal links are previously set invalid.
        if (link == header_link::terminal)
            continue;

        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_valid(link))
        {
            fault(error::batch4);
            return;
        }

        notify_block(system::error::block_success, height, link, false);
    }

    // All batched are processed, and since strand-protected is safe to clear.
    batched_.clear();

    // May have grown to maximum_concurrency and never used again once current.
    if (is_current(true))
        batched_.shrink_to_fit();

    // Purge all signature batch tables.
    if (!query.purge_signatures())
    {
        fault(error::batch5);
        return;
    }

    LOGN("Batch verification begin.");
}

void chaser_validate::notify_block(const code& ec, size_t height, 
    const header_link& link, bool bypass) NOEXCEPT
{
    if (ec)
    {
        // INVALID BLOCK (not a fault)
        notify(ec, chase::unvalid, link);
        fire(events::block_unconfirmable, height);
        LOGR("Invalid block [" << height << "] " << ec.message());
        return;
    }

    // VALID BLOCK
    notify(ec, chase::valid, possible_wide_cast<height_t>(height));
    fire(events::block_validated, height);
    LOGV("Block validated: " << height << (bypass ? " (bypass)" : ""));
}

// private
// ----------------------------------------------------------------------------

chain::signatures chaser_validate::get_capture(
    const header_link& link) NOEXCEPT
{
    if (!batch_signatures_ || is_current(link))
        return { false };

    const auto sequence = to_shared<atomic_counter>();
    const auto lock = emplace_shared<shared_lock>(mutex_);
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

void chaser_validate::do_log(const chain::script& ) NOEXCEPT
{
    // Enable for a game of whack-a-mole.
    ////LOGA("Sigop @ " << ctx.height << " -> "
    ////    << missed.to_string(chain::flags::all_rules));
}

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
    if (!set) fault(error::batch6);
    return set;
}

bool chaser_validate::do_schnorr(const hash_digest& digest,
    const ec_xonly& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++schnorr_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch7);
    return set;
}

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

bool chaser_validate::do_multisig(const hash_digest& digest,
    const ec_compresseds& points, const ec_signatures& signs,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    BC_ASSERT(points.size() == signs.size());

    multisig_ += points.size();
    const auto set = archive().set_signatures(digest, points, signs,
        (*sequence)++, link);
    if (!set) fault(error::batch8);
    return set;
}

bool chaser_validate::do_threshold(const threshold_group& group,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    threshold_ += group.entries.size();
    const auto set = archive().set_signatures(group, (*sequence)++, link);
    if (!set) fault(error::batch9);
    return set;
}

BC_POP_WARNING()
BC_POP_WARNING()

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

// Overrides due to independent priority thread pool
// ----------------------------------------------------------------------------

void chaser_validate::stopping(const code& ec) NOEXCEPT
{
    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    validation_threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_validate::stop() NOEXCEPT
{
    if (!validation_threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

network::asio::strand& chaser_validate::strand() NOEXCEPT
{
    return validation_strand_;
}

bool chaser_validate::stranded() const NOEXCEPT
{
    return validation_strand_.running_in_this_thread();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
