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
    defer_(node.node_settings().defer_validation),
    filter_(!defer_ && node.archive().filter_enabled())
{
}

code chaser_validate::start() NOEXCEPT
{
    if (!node_settings().headers_first)
        return error::success;

    const auto& query = archive();
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
    // Stop when suspended as write error des not terminate asynchronous loop.
    while ((backlog_ < maximum_backlog_) && !closed() && !suspended())
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Must exit on unassociated so they are not set valid in bypass.
        // Given height-based iteration, any block state may be enountered.
        if (ec == database::error::unassociated)
            return;

        const auto bypass = defer_ || is_under_checkpoint(height) ||
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
    backlog_.fetch_add(one, std::memory_order_relaxed);
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
    else if ((ec = validate(bypass, *block, link, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate5;
    }

    complete_block(ec, link, ctx.height, bypass);

    // Prevent stall by posting internal event, avoiding external handlers.
    if (is_one(backlog_.fetch_sub(one, std::memory_order_relaxed)))
        handle_chase(error::success, chase::bump, height_t{});
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

code chaser_validate::validate(bool bypass, const chain::block& block,
    const database::header_link& link, const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();

    if (!bypass)
    {
        code ec{};

        // Skips identity validation (performed in downloader).
        // This can be performed in downloader as well but moving it here with
        // more order than required allows the downloader to avoid block parse
        // which significantly reduces memory consumption and CPU during sync.
        // This slightly increases check() computation under full validation
        // because a redundant malleation guard is required when downloading.
        if (((ec = block.check(false))) || ((ec = block.check(ctx, false))))
            return ec;

        if ((ec = block.accept(ctx, subsidy_interval_, initial_subsidy_)))
            return ec;

        if (batch_signatures_)
        {
            using namespace chain;
            std::atomic<size_t> set{};
            code capture_ec{};

            const auto to_events = [](opcode op) NOEXCEPT
            {
                switch (op)
                {
                    // ecdsa single (checksig/verify).
                    case opcode::checksigverify:
                        return events::checksigverify;

                    // ecdsa multiple (checkmultisig/verify).
                    case opcode::checkmultisigverify:
                        return events::checkmultisigverify;

                    // schnorr single (op_checksigadd|op_checksig/verify).
                    case opcode::checksigadd:
                        return events::checksigadd;

                    // schnorr multiple (multisig pattern).
                    case opcode::checksig:
                        return events::checksig;

                    // schnorr multiple (is_threshold).
                    case opcode::numequal:
                        return events::numequal;
                    case opcode::numequalverify:
                        return events::numequalverify;
                    case opcode::numnotequal:
                        return events::numnotequal;
                    case opcode::lessthan:
                        return events::lessthan;
                    case opcode::greaterthan:
                        return events::greaterthan;
                    case opcode::lessthanorequal:
                        return events::lessthanorequal;
                    case opcode::greaterthanorequal:
                        return events::greaterthanorequal;
                    case opcode::within:
                        return events::within;

                    // should be no path to this.
                    default:
                        return events::unknown;
                }
            };

            const signatures capture
            {
                // Enable/disable capture.
                .enabled = batch_signatures_,

                .log = [&](const script& missed) NOEXCEPT
                {
                    ////LOGA("Sigop @ " << ctx.height << " -> "
                    ////    << missed.to_string(chain::flags::all_rules));
                },
                .fire = [&](signatures::miss miss, size_t count) NOEXCEPT
                {
                    switch (miss)
                    {
                        case signatures::miss::ecdsa:
                            missed_ecdsa_ += count;
                            ////fire(events::missed_ecdsa, ctx.height);
                            break;
                        case signatures::miss::multisig:
                            missed_multisig_ += count;
                            ////fire(events::missed_multisig, ctx.height);
                            break;
                        case signatures::miss::schnorr:
                            missed_schnorr_ += count;
                            ////fire(events::missed_schnorr, ctx.height);
                            break;
                        case signatures::miss::overflow:
                            // Misses overflow to single, so not called.
                            // This is instead used to reflect cache failure.
                            missed_threshold_ += count;
                            ////fire(events::missed_overflow, ctx.height);
                            break;

                        // should be no path to this.
                        default:
                            BC_ASSERT_MSG(false, "unknown signatures::miss");
                    }
                },
                .ecdsa = [&](const hash_digest& digest,
                    const ec_compressed& point,
                    const ec_signature& sign) NOEXCEPT
                {
                    ++ecdsa_;
                    ////fire(to_events(opcode::checksigverify), ctx.height);
                    return query.set_signature(digest, point, sign, link);
                },
                .schnorr = [&](const hash_digest& digest, const ec_xonly& point,
                    const ec_signature& sign) NOEXCEPT
                {
                    ++schnorr_;
                    ////fire(to_events(opcode::checksigadd), ctx.height);
                    return query.set_signature(digest, point, sign, link);
                },
                .multisig = [&](const hash_digest& digest,
                    const ec_compresseds& points,
                    const ec_signatures& signs) NOEXCEPT
                {
                    BC_ASSERT(points.size() == signs.size());
                    multisig_ += points.size();
                    ////fire(to_events(opcode::checkmultisigverify), ctx.height);
                    return query.set_signatures(digest, points, signs, set, link);
                },
                .threshold = [&](
                    const signatures::threshold_entries& group) NOEXCEPT
                {
                    // Sets condition to opcode::checksig for all required.
                    threshold_ += group.entries.size();
                    ////fire(to_events(group.condition), ctx.height);

                    // Script always processing proceeds as if batch succeeded.
                    if (!query.set_signatures(group, set, link))
                        capture_ec = fault(error::capture_fault);
                }
            };

            // Prioritize validation failure over capture failure.
            if ((ec = block.connect(ctx, capture)))
                return ec;

            // TODO: repost block (link) to work queue in complete_block
            // TODO: based on error::capture_fault.
            if (capture_ec)
                return capture_ec;

            const auto log_capture = [&](std::string_view name,
                size_t captured, size_t missed) NOEXCEPT
            {
                if (!to_bool(captured) && !to_bool(missed)) return;
                const auto ratio = (100.0f * captured) / (captured + missed);
                const auto rate = (boost_format("%.4f") % ratio).str();
                LOGA("Efficiency " << name << rate << "% = " << captured
                    << "/(" << captured << "+" << missed << ")");
            };

            log_capture("ecdsa.... ", ecdsa_,     missed_ecdsa_);
            log_capture("multisig. ", multisig_,  missed_multisig_);
            log_capture("schnorr.. ", schnorr_,   missed_schnorr_);
            log_capture("threshold ", threshold_, missed_threshold_);
        }
        else
        {
            if ((ec = block.connect(ctx)))
                return ec;
        }

        // Prevouts optimize confirmation.
        if (!query.set_prevouts(link, block))
            return error::validate6;
    }

    if (!query.set_filter_body(link, block))
        return error::validate7;

    // Valid must be set after set_prevouts and set_filter_body.
    if (!bypass && !query.set_block_valid(link))
        return error::validate8;

    return error::success;
}

// May be either concurrent or stranded.
void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height, bool bypass) NOEXCEPT
{
    if (ec)
    {
        // Node errors are fatal.
        if (node::error::error_category::contains(ec))
        {
            // fault(ec) initiates recovery if caused by disk full condition.
            LOGF("Fault validating [" << height << "] " << ec.message());
            fault(ec);
            return;
        }

        // INVALID BLOCK (not a fault)
        notify(ec, chase::unvalid, link);
        fire(events::block_unconfirmable, height);
        LOGR("Invalid block [" << height << "] " << ec.message());
        return;
    }

    // VALID BLOCK
    // Under deferral there is no state change, but downloads will stall unless
    // the window is closed out, so notify the check chaser of the increment.
    notify(ec, chase::valid, possible_wide_cast<height_t>(height));

    if (!defer_)
    {
        fire(events::block_validated, height);
        LOGV("Block validated: " << height << (bypass ? " (bypass)" : ""));
    }
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
