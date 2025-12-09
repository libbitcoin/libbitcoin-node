/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_INTERFACES_BITCOIND_HPP
#define LIBBITCOIN_NODE_INTERFACES_BITCOIND_HPP

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
namespace interface {

struct bitcoind_methods
{
    static constexpr std::tuple methods
    {
        /// Blockchain methods.
        method<"getbestblockhash">{},
        method<"getblock", string_t, optional<0_u32>>{ "blockhash", "verbosity" },
        method<"getblockchaininfo">{},
        method<"getblockcount">{},
        method<"getblockfilter", string_t, optional<"basic"_t>>{ "blockhash", "filtertype" },
        method<"getblockhash", number_t>{ "height" },
        method<"getblockheader", string_t, optional<true>>{ "blockhash", "verbose" },
        method<"getblockstats", string_t, optional<empty::array>>{ "hash_or_height", "stats" },
        method<"getchaintxstats", optional<-1_i32>, optional<""_t>>{ "nblocks", "blockhash" },
        method<"getchainwork">{},
        method<"gettxout", string_t, number_t, optional<true>>{ "txid", "n", "include_mempool" },
        method<"gettxoutsetinfo">{},
        method<"pruneblockchain", number_t>{ "height" },
        method<"savemempool">{},
        method<"scantxoutset", string_t, optional<empty::array>>{ "action", "scanobjects" },
        method<"verifychain", optional<4_u32>, optional<288_u32>>{ "checklevel", "nblocks" },
        method<"verifytxoutset", string_t>{ "input_verify_flag" },

        /// Control methods.
        method<"getmemoryinfo", optional<"stats"_t>>{ "mode" },
        method<"getrpcinfo">{},
        method<"help", optional<""_t>>{ "command" },
        method<"logging", optional<"*"_t>>{ "include" },
        method<"stop">{},
        method<"uptime">{},

        /// Mining methods.
        method<"getblocktemplate", optional<empty::object>>{ "template_request" },
        method<"getmininginfo">{},
        method<"getnetworkhashps", optional<120_u32>, optional<-1_i32>>{ "nblocks", "height" },
        method<"prioritisetransaction", string_t, number_t, number_t>{ "txid", "dummy", "priority_delta" },
        method<"submitblock", string_t, optional<""_t>>{ "block", "parameters" },

        /// Network methods.
        method<"addnode", string_t, string_t>{ "node", "command" },
        method<"clearbanned">{},
        method<"disconnectnode", string_t, optional<-1_i32>>{ "address", "nodeid" },
        method<"getaddednodeinfo", optional<false>, optional<true>, optional<""_t>>{ "include_chain_info", "dns", "addnode" },
        method<"getconnectioncount">{},
        method<"getnetworkinfo">{},
        method<"getpeerinfo">{},
        method<"listbanned">{},
        method<"ping">{},
        method<"setban", string_t, string_t, optional<86400_u32>, optional<false>, optional<""_t>>{ "addr", "command", "bantime", "absolute", "reason" },
        method<"setnetworkactive", boolean_t>{ "state" },

        /// Rawtransactions methods.
        method<"combinerawtransaction", array_t>{ "txs" },
        method<"createrawtransaction", array_t, object_t, optional<0_u32>, optional<false>>{ "inputs", "outputs", "locktime", "replaceable" },
        method<"decoderawtransaction", string_t>{ "hexstring" },
        method<"fundrawtransaction", string_t, optional<empty::object>>{ "rawtx", "options" },
        method<"getrawtransaction", string_t, optional<0_u32>, optional<""_t>>{ "txid", "verbose", "blockhash" },
        method<"sendrawtransaction", string_t, optional<0_u32>>{ "hexstring", "maxfeerate" },
        method<"signrawtransactionwithkey", string_t, optional<empty::array>, optional<empty::array>, optional<"ALL|FORKID"_t>>{ "hexstring", "privkeys", "prevtxs", "sighashtype" },
        method<"testmempoolaccept", array_t, optional<0_u32>>{ "rawtxs", "maxfeerate" },
        method<"testrawtransaction", string_t>{ "rawtx" },

        /// Util methods (node-related).
        method<"createmultisig", number_t, array_t>{ "nrequired", "keys" },
        method<"decodepsbt", string_t>{ "psbt" },
        method<"decodescript", string_t>{ "hex" },
        method<"estimaterawfee", number_t, optional<"unset"_t>>{ "conf_target", "estimate_mode" },
        method<"getdescriptorinfo", string_t>{ "descriptor" },
        method<"validateaddress", string_t>{ "address" },

        /// Wallet methods (unsupported).
        method<"abandontransaction", string_t>{ "txid" },
        method<"addmultisigaddress", number_t, array_t, optional<""_t>, optional<"legacy"_t>>{ "nrequired", "keys", "label", "address_type" },
        method<"backupwallet", string_t>{ "destination" },
        method<"bumpfee", string_t, optional<empty::object>>{ "txid", "options" },
        method<"createwallet", string_t, optional<false>, optional<false>, optional<false>, optional<""_t>, optional<false>, optional<false>, optional<false>, optional<"set"_t>>{ "wallet_name", "disable_private_keys", "blank", "passphrase", "avoid_reuse", "descriptors", "load_on_startup", "external_signer", "change_type" },
        method<"dumpprivkey", string_t>{ "address" },
        method<"dumpwallet", string_t>{ "filename" },
        method<"encryptwallet", string_t>{ "passphrase" },
        method<"getaddressinfo", string_t>{ "address" },
        method<"getbalances">{},
        method<"getnewaddress", optional<""_t>, optional<"legacy"_t>>{ "label", "address_type" },
        method<"getreceivedbyaddress", string_t, optional<0>>{ "address", "minconf" },
        method<"getreceivedbylabel", string_t, optional<0>>{ "label", "minconf" },
        method<"gettransaction", string_t, optional<false>, optional<false>>{ "txid", "include_watchonly", "verbose" },
        method<"getunconfirmedbalance">{},
        method<"getwalletinfo">{},
        method<"importaddress", string_t, optional<""_t>, optional<false>, optional<true>>{ "address", "label", "rescan", "p2sh" },
        method<"importmulti", array_t, optional<empty::object>>{ "requests", "options" },
        method<"importprivkey", string_t, optional<""_t>, optional<false>>{ "privkey", "label", "rescan" },
        method<"importprunedfunds", string_t, string_t>{ "rawtransaction", "txoutproof" },
        method<"importpubkey", string_t, optional<""_t>, optional<false>>{ "pubkey", "label", "rescan" },
        method<"importwallet", string_t>{ "filename" },
        method<"keypoolrefill", optional<100>>{ "newsize" },
        method<"listaddressgroupings", optional<1>, optional<false>>{ "minconf", "include_watchonly" },
        method<"listlabels", optional<"receive"_t>>{ "purpose" },
        method<"listlockunspent">{},
        method<"listreceivedbyaddress", optional<1>, optional<false>, optional<false>, optional<""_t>>{ "minconf", "include_empty", "include_watchonly", "address_filter" },
        method<"listreceivedbylabel", optional<1>, optional<false>, optional<false>>{ "minconf", "include_empty", "include_watchonly" },
        method<"listtransactions", optional<""_t>, optional<10>, optional<0>, optional<false>>{ "label", "count", "skip", "include_watchonly" },
        method<"listunspent", optional<1>, optional<empty::array>, optional<true>, optional<false>>{ "minconf", "addresses", "include_unsafe", "query_options" },
        method<"loadwallet", string_t, optional<false>>{ "filename", "load_on_startup" },
        method<"lockunspent", boolean_t, optional<empty::array>>{ "unlock", "transactions" },
        method<"removeprunedfunds", string_t>{ "txid" },
        method<"rescanblockchain", optional<0>>{ "start_height" },
        method<"send", object_t, optional<empty::object>>{ "outputs", "options" },
        method<"sendmany", string_t, object_t, optional<1>, optional<""_t>, optional<""_t>, optional<false>, optional<false>, optional<25>, optional<"unset"_t>, optional<false>, optional<0>>{ "dummy", "outputs", "minconf", "comment", "comment_to", "subtractfeefrom", "replaceable", "conf_target", "estimate_mode", "avoid_reuse", "fee_rate" },
        method<"sendtoaddress", string_t, number_t, optional<""_t>, optional<""_t>, optional<false>, optional<25>, optional<"unset"_t>, optional<false>, optional<0>, optional<false>>{ "address", "amount", "comment", "comment_to", "subtractfeefromamount", "conf_target", "estimate_mode", "avoid_reuse", "fee_rate", "verbose" },
        method<"setlabel", string_t, string_t>{ "address", "label" },
        method<"settxfee", number_t>{ "amount" },
        method<"signmessage", string_t, string_t>{ "address", "message" },
        method<"signmessagewithprivkey", string_t, string_t>{ "privkey", "message" },
        method<"syncwithvalidationinterfacequeue">{},
        method<"unloadwallet", optional<""_t>, optional<false>>{ "wallet_name", "load_on_startup" },
        method<"walletcreatefundedpsbt", optional<empty::array>, optional<empty::object>, optional<0>, optional<empty::object>>{ "inputs", "outputs", "locktime", "options" },
        method<"walletlock">{},
        method<"walletpassphrase", string_t, number_t>{ "passphrase", "timeout" },
        method<"walletprocesspsbt", string_t, optional<false>, optional<false>, optional<false>>{ "psbt", "sign", "bip32derivs", "complete" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using getbestblockhash = at<0>;
    using getblock = at<1>;
    using getblockchaininfo = at<2>;
    using getblockcount = at<3>;
    using getblockfilter = at<4>;
    using getblockhash = at<5>;
    using getblockheader = at<6>;
    using getblockstats = at<7>;
    using getchaintxstats = at<8>;
    using getchainwork = at<9>;
    using gettxout = at<10>;
    using gettxoutsetinfo = at<11>;
    using pruneblockchain = at<12>;
    using savemempool = at<13>;
    using scantxoutset = at<14>;
    using verifychain = at<15>;
    using verifytxoutset = at<16>;
    using getmemoryinfo = at<17>;
    using getrpcinfo = at<18>;
    using help = at<19>;
    using logging = at<20>;
    using stop = at<21>;
    using uptime = at<22>;
    using getblocktemplate = at<23>;
    using getmininginfo = at<24>;
    using getnetworkhashps = at<25>;
    using prioritisetransaction = at<26>;
    using submitblock = at<27>;
    using addnode = at<28>;
    using clearbanned = at<29>;
    using disconnectnode = at<30>;
    using getaddednodeinfo = at<31>;
    using getconnectioncount = at<32>;
    using getnetworkinfo = at<33>;
    using getpeerinfo = at<34>;
    using listbanned = at<35>;
    using ping = at<36>;
    using setban = at<37>;
    using setnetworkactive = at<38>;
    using combinerawtransaction = at<39>;
    using createrawtransaction = at<40>;
    using decoderawtransaction = at<41>;
    using fundrawtransaction = at<42>;
    using getrawtransaction = at<43>;
    using sendrawtransaction = at<44>;
    using signrawtransactionwithkey = at<45>;
    using testmempoolaccept = at<46>;
    using testrawtransaction = at<47>;
    using createmultisig = at<48>;
    using decodepsbt = at<49>;
    using decodescript = at<50>;
    using estimaterawfee = at<51>;
    using getdescriptorinfo = at<52>;
    using validateaddress = at<53>;
    using abandontransaction = at<54>;
    using addmultisigaddress = at<55>;
    using backupwallet = at<56>;
    using bumpfee = at<57>;
    using createwallet = at<58>;
    using dumpprivkey = at<59>;
    using dumpwallet = at<60>;
    using encryptwallet = at<61>;
    using getaddressinfo = at<62>;
    using getbalances = at<63>;
    using getnewaddress = at<64>;
    using getreceivedbyaddress = at<65>;
    using getreceivedbylabel = at<66>;
    using gettransaction = at<67>;
    using getunconfirmedbalance = at<68>;
    using getwalletinfo = at<69>;
    using importaddress = at<70>;
    using importmulti = at<71>;
    using importprivkey = at<72>;
    using importprunedfunds = at<73>;
    using importpubkey = at<74>;
    using importwallet = at<75>;
    using keypoolrefill = at<76>;
    using listaddressgroupings = at<77>;
    using listlabels = at<78>;
    using listlockunspent = at<79>;
    using listreceivedbyaddress = at<80>;
    using listreceivedbylabel = at<81>;
    using listtransactions = at<82>;
    using listunspent = at<83>;
    using loadwallet = at<84>;
    using lockunspent = at<85>;
    using removeprunedfunds = at<86>;
    using rescanblockchain = at<87>;
    using send = at<88>;
    using sendmany = at<89>;
    using sendtoaddress = at<90>;
    using setlabel = at<91>;
    using settxfee = at<92>;
    using signmessage = at<93>;
    using signmessagewithprivkey = at<94>;
    using syncwithvalidationinterfacequeue = at<95>;
    using unloadwallet = at<96>;
    using walletcreatefundedpsbt = at<97>;
    using walletlock = at<98>;
    using walletpassphrase = at<99>;
    using walletprocesspsbt = at<100>;
};

} // namespace interface
} // namespace node
} // namespace libbitcoin

#endif
