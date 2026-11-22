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
#ifndef LIBBITCOIN_NODE_CHASE_HPP
#define LIBBITCOIN_NODE_CHASE_HPP

#include <bitcoin/database.hpp>
#include <bitcoin/node/events.hpp>

namespace libbitcoin {
namespace node {

/// Event desubscriber key type.
using object_key = uint64_t;

/// Event payload types.
using count_t = size_t;
using height_t = size_t;
using peer_t = uint64_t;
using object_t = object_key;
using header_t = database::header_link::integer;
using transaction_t = database::tx_link::integer;

enum class chase
{
    /// Work shuffling.
    /// -----------------------------------------------------------------------

    /// Chasers directed to start operating.
    /// Issued by 'full_node' and handled by 'download', 'validate', 'confirm'.
    start,

    /// Disk space is limited.
    /// Issued by 'full_node' and handled by 'storage'.
    space,

    /// Take a snapshot.
    /// Issued by 'confirm' and handled by 'snapshot'.
    snap,

    /// Chaser directed to attempt start from current position.
    /// Issued by 'organize' and handled by 'download', 'validate', 'confirm'.
    bump,

    /// Channels (all) directed to stop with given code.
    /// Issued by 'full_node' and handled by 'observer'.
    suspend,

    /// Chasers (all) directed to resume following suspend.
    /// Issued by 'full_node' and handled by 'download', 'validate', 'confirm'.
    resume,

    /// Node is recovering from disk full condition.
    /// Issued by 'storage' and handled by 'validate'.
    unfull,

    /// Channel starved for work.
    /// Issued by 'block_in_31800' and handled by 'download'.
    starved,

    /// Channel (slow) directed to split work and stop.
    /// Issued by 'download' and handled by 'block_in_31800'.
    split,

    /// Channels (all with work) directed to split work and stop.
    /// Issued by 'download' and handled by 'block_in_31800'.
    stall,

    /// Channels (all with work) directed to drop work and stop.
    /// Issued by 'download' and handled by 'block_in_31800'.
    purge,

    /// Channels (all) directed to write work count to log.
    /// Issued by 'executor' and handled by 'block_in_31800'.
    report,

    /// Candidate Chain.
    /// -----------------------------------------------------------------------

    /// New candidate branch exists from given branch point.
    /// Issued by 'header' and handled by 'download'.
    headers,

    /// New candidate headers without txs exist.
    /// Issued by 'download' and handled by 'block_in_31800'.
    download,

    /// Candidate chain has been reorganized (branched below its top).
    /// Issued by 'organize' and handled by 'download', 'validate', 'confirm'.
    regressed,

    /// unchecked, unvalid or unconfirmable was handled.
    /// Issued by 'organize' and handled by 'download', 'validate', 'confirm'.
    disorganized,

    /// Check/Identify.
    /// -----------------------------------------------------------------------

    /// Block has been downloaded, checked and stored.
    /// Issued by 'block_in_31800', handled by 'download', 'validate'.
    /// Populate is bypassed for checkpoint/milestone blocks.
    checked,

    /// Downloaded block has failed check.
    /// Issued by 'block_in_31800', 'transaction' and handled by 'organize'.
    unchecked,

    /// Downloaded window is completed by check.
    /// Issued by 'download' and handled by 'validate'.
    windowed,

    /// Accept/Connect.
    /// -----------------------------------------------------------------------

    /// Branch has become valid.
    /// Issued by 'validate' and handled by 'download', 'confirm'.
    valid,

    /// Checked block has failed validation.
    /// Issued by 'validate' and handled by 'organize'.
    unvalid,

    /// Confirm (block).
    /// -----------------------------------------------------------------------

    /// Connected block has become confirmable.
    /// Issued by 'confirm' and handled by none.
    confirmable,

    /// Connected block has failed confirmability.
    /// Issued by 'confirm' and handled by 'organize'.
    unconfirmable,

    /// Confirm (chain).
    /// -----------------------------------------------------------------------

    /// Current block has been organized.
    /// Issued by 'confirm' and handled by 'transaction',
    /// 'protocol_header/block_out'.
    block,

    /// Current block has been settled in pool.
    /// Issued by 'transaction' and handled by 'snapshot'.
    block_settled,

    /// Store has been pruned (once, following first settled block).
    /// Issued by 'snapshot' and handled by 'transaction'.
    pruned,

    /// Confirmed chain is no longer current.
    /// Issued by 'confirm' and handled by 'protocol_transaction_out'.
    stale,

    /// Confirmable block has been confirmed.
    /// Issued by 'confirm' and handled by 'transaction'.
    organized,

    /// Previously confirmed block has been unconfirmed.
    /// Issued by 'confirm' and handled by 'transaction'.
    reorganized,

    /// Mining.
    /// -----------------------------------------------------------------------

    /// Transaction has been added to pool.
    /// Issued by 'transaction' and handled by [clients].
    transaction,

    /// Pooled transaction has been relayed to word of slots, in order.
    /// Issued by 'transaction' and handled by 'protocol_transaction_out'.
    relay,

    /// Candidate block (template) has been created, upon request.
    /// Issued by 'template' and handled by [miners].
    template_,

    /// Stop.
    /// -----------------------------------------------------------------------

    /// Service is stopping, accompanied by error::service_stopped.
    stop
};

/// Event payloads, one per chase value, declared in chase order.
namespace chases {

struct start
{
    static constexpr chase id{ chase::start };
};

struct space
{
    static constexpr chase id{ chase::space };
};

struct snap
{
    static constexpr chase id{ chase::snap };
    height_t height;
};

struct bump
{
    static constexpr chase id{ chase::bump };
    height_t height;
};

struct suspend
{
    static constexpr chase id{ chase::suspend };
};

struct resume
{
    static constexpr chase id{ chase::resume };
};

struct unfull
{
    static constexpr chase id{ chase::unfull };
};

struct starved
{
    static constexpr chase id{ chase::starved };
    object_t channel;
};

struct split
{
    static constexpr chase id{ chase::split };
    object_t channel;
};

struct stall
{
    static constexpr chase id{ chase::stall };
    object_t channel;
};

struct purge
{
    static constexpr chase id{ chase::purge };
    height_t branch_point;
};

struct report
{
    static constexpr chase id{ chase::report };
    count_t sequence;
};

struct headers
{
    static constexpr chase id{ chase::headers };
    height_t branch_point;
};

struct download
{
    static constexpr chase id{ chase::download };
    count_t count;
};

struct regressed
{
    static constexpr chase id{ chase::regressed };
    height_t branch_point;
};

struct disorganized
{
    static constexpr chase id{ chase::disorganized };
    height_t branch_point;
};

struct checked
{
    static constexpr chase id{ chase::checked };
    height_t height;
};

struct unchecked
{
    static constexpr chase id{ chase::unchecked };
    header_t link;
};

struct windowed
{
    static constexpr chase id{ chase::windowed };
    height_t height;
};

struct valid
{
    static constexpr chase id{ chase::valid };
    height_t height;
};

struct unvalid
{
    static constexpr chase id{ chase::unvalid };
    header_t link;
};

struct confirmable
{
    static constexpr chase id{ chase::confirmable };
    header_t link;
};

struct unconfirmable
{
    static constexpr chase id{ chase::unconfirmable };
    header_t link;
};

struct block
{
    static constexpr chase id{ chase::block };
    header_t link;
};

struct block_settled
{
    static constexpr chase id{ chase::block_settled };
    header_t link;
};

struct pruned
{
    static constexpr chase id{ chase::pruned };
    header_t link;
};

struct stale
{
    static constexpr chase id{ chase::stale };
};

struct organized
{
    static constexpr chase id{ chase::organized };
    header_t link;
};

struct reorganized
{
    static constexpr chase id{ chase::reorganized };
    header_t link;
};

struct transaction
{
    static constexpr chase id{ chase::transaction };
    transaction_t link;
};

struct relay
{
    static constexpr chase id{ chase::relay };
    transaction_t link;
    uint64_t slots;
};

struct template_
{
    static constexpr chase id{ chase::template_ };
    height_t height;
};

struct stop
{
    static constexpr chase id{ chase::stop };
};

} // namespace chases

/// Alternative position is the chase value, so the event carries its own type.
using event_value = std::variant
<
    chases::start,
    chases::space,
    chases::snap,
    chases::bump,
    chases::suspend,
    chases::resume,
    chases::unfull,
    chases::starved,
    chases::split,
    chases::stall,
    chases::purge,
    chases::report,
    chases::headers,
    chases::download,
    chases::regressed,
    chases::disorganized,
    chases::checked,
    chases::unchecked,
    chases::windowed,
    chases::valid,
    chases::unvalid,
    chases::confirmable,
    chases::unconfirmable,
    chases::block,
    chases::block_settled,
    chases::pruned,
    chases::stale,
    chases::organized,
    chases::reorganized,
    chases::transaction,
    chases::relay,
    chases::template_,
    chases::stop
>;

template <size_t... Index>
constexpr bool is_chase_ordered(std::index_sequence<Index...>) NOEXCEPT
{
    return ((std::variant_alternative_t<Index, event_value>::id ==
        static_cast<chase>(Index)) && ...);
}

static_assert(is_chase_ordered(std::make_index_sequence<
    std::variant_size_v<event_value>>{}));

/// The event's chase value.
constexpr chase to_chase(const event_value& value) NOEXCEPT
{
    return static_cast<chase>(value.index());
}

/// The event's payload, guarded by the alternative ordering above.
template <chase Event>
constexpr const auto& to_payload(const event_value& value) NOEXCEPT
{
    return std::get<to_value(Event)>(value);
}

} // namespace node
} // namespace libbitcoin

#endif
