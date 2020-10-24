// Copyright (c)2020, The Arqma Network
// Copyright (c)2020, ArqTras
// Portions of this software are available under BSD-3 license. Please see ORIGINAL-LICENSE for details
// All rights reserved.
// Authors and copyright holders give permission for following:
// 1. Redistribution and use in source and binary forms WITHOUT modification.
// 2. Modification of the source form for your own personal use.
// As long as the following conditions are met:
// 3. You must not distribute modified copies of the work to third parties. This includes
//    posting the work online, or hosting copies of the modified work for download.
// 4. Any derivative version of this work is also covered by this license, including point 8.
// 5. Neither the name of the copyright holders nor the names of the authors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 6. You agree that this licence is governed by and shall be construed in accordance
//    with the laws of England and Wales.
// 7. You agree to submit all disputes arising out of or in connection with this licence
//    to the exclusive jurisdiction of the Courts of England and Wales.
// Authors and copyright holders agree that:
// 8. This licence expires and the work covered by it is released into the
//    public domain on 1st of March 2021

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once
#include <string>

namespace ascii_wallet
{
const char* USAGE_START_MINING("start_mining [<number_of_threads>] [bg_mining] [ignore_battery]");
const char* USAGE_SET_DAEMON("set_daemon <host>[:<port>] [trusted|untrusted]");
const char* USAGE_SHOW_BALANCE("balance [detail]");
const char* USAGE_INCOMING_TRANSFERS("incoming_transfers [available|unavailable] [verbose] [uses] [index=<N1>[,<N2>[,...]]]");
const char* USAGE_PAYMENTS("payments <PID_1> [<PID_2> ... <PID_N>]");
const char* USAGE_PAYMENT_ID("payment_id");
const char* USAGE_TRANSFER("transfer [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] (<URI> | <address> <amount>) [<payment_id>]");
const char* USAGE_LOCKED_TRANSFER("locked_transfer [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] (<URI> | <addr> <amount>) <lockblocks> [<payment_id>]");
const char* USAGE_LOCKED_SWEEP_ALL("locked_sweep_all [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] <address> <lockblocks> [<payment_id>]");
const char* USAGE_SWEEP_ALL("sweep_all [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] [outputs=<N>] <address> [<payment_id>]");
const char* USAGE_SWEEP_BELOW("sweep_below <amount_threshold> [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] <address> [<payment_id>]");
const char* USAGE_SWEEP_SINGLE("sweep_single [<priority>] [<ring_size>] [outputs=<N>] <key_image> <address> [<payment_id>]");
const char* USAGE_DONATE("donate [index=<N1>[,<N2>,...]] [<priority>] [<ring_size>] <amount> [<payment_id>]");
const char* USAGE_SIGN_TRANSFER("sign_transfer [export_raw]");
const char* USAGE_SET_LOG("set_log <level>|{+,-,}<categories>");
const char* USAGE_ACCOUNT("account\n"
                          "  account new <label text with white spaces allowed>\n"
                          "  account switch <index> \n"
                          "  account label <index> <label text with white spaces allowed>\n"
                          "  account tag <tag_name> <account_index_1> [<account_index_2> ...]\n"
                          "  account untag <account_index_1> [<account_index_2> ...]\n"
                          "  account tag_description <tag_name> <description>");
const char* USAGE_ADDRESS("address [ new <label text with white spaces allowed> | all | <index_min> [<index_max>] | label <index> <label text with white spaces allowed>]");
const char* USAGE_INTEGRATED_ADDRESS("integrated_address [<payment_id> | <address>]");
const char* USAGE_ADDRESS_BOOK("address_book [(add ((<address> [pid <id>])|<integrated address>) [<description possibly with whitespaces>])|(delete <index>)]");
const char* USAGE_SET_VARIABLE("set <option> [<value>]");
const char* USAGE_GET_TX_KEY("get_tx_key <txid>");
const char* USAGE_SET_TX_KEY("set_tx_key <txid> <tx_key>");
const char* USAGE_CHECK_TX_KEY("check_tx_key <txid> <txkey> <address>");
const char* USAGE_GET_TX_PROOF("get_tx_proof <txid> <address> [<message>]");
const char* USAGE_CHECK_TX_PROOF("check_tx_proof <txid> <address> <signature_file> [<message>]");
const char* USAGE_GET_SPEND_PROOF("get_spend_proof <txid> [<message>]");
const char* USAGE_CHECK_SPEND_PROOF("check_spend_proof <txid> <signature_file> [<message>]");
const char* USAGE_GET_RESERVE_PROOF("get_reserve_proof (all|<amount>) [<message>]");
const char* USAGE_CHECK_RESERVE_PROOF("check_reserve_proof <address> <signature_file> [<message>]");
const char* USAGE_SHOW_TRANSFER("show_transfer <txid>");
const char* USAGE_SHOW_TRANSFERS("show_transfers [in|out|pending|failed|pool|coinbase] [index=<N1>[,<N2>,...]] [<min_height> [<max_height>]]");
const char* USAGE_EXPORT_TRANSFERS("export_transfers [in|out|all|pending|failed|coinbase] [index=<N1>[,<N2>,...]] [<min_height> [<max_height>]] [output=<filepath>]");
const char* USAGE_UNSPENT_OUTPUTS("unspent_outputs [index=<N1>[,<N2>,...]] [<min_amount> [<max_amount>]]");
const char* USAGE_SET_TX_NOTE("set_tx_note <txid> [free text note]");
const char* USAGE_GET_TX_NOTE("get_tx_note <txid>");
const char* USAGE_GET_DESCRIPTION("get_description");
const char* USAGE_SET_DESCRIPTION("set_description [free text note]");
const char* USAGE_SIGN("sign <filename>");
const char* USAGE_VERIFY("verify <filename> <address> <signature>");
const char* USAGE_EXPORT_KEY_IMAGES("export_key_images <filename>");
const char* USAGE_IMPORT_KEY_IMAGES("import_key_images <filename>");
const char* USAGE_HW_RECONNECT("hw_reconnect");
const char* USAGE_EXPORT_OUTPUTS("export_outputs <filename>");
const char* USAGE_IMPORT_OUTPUTS("import_outputs <filename>");
const char* USAGE_MAKE_MULTISIG("make_multisig <threshold> <string1> [<string>...]");
const char* USAGE_FINALIZE_MULTISIG("finalize_multisig <string> [<string>...]");
const char* USAGE_EXCHANGE_MULTISIG_KEYS("exchange_multisig_keys <string> [<string>...]");
const char* USAGE_EXPORT_MULTISIG_INFO("export_multisig_info <filename>");
const char* USAGE_IMPORT_MULTISIG_INFO("import_multisig_info <filename> [<filename>...]");
const char* USAGE_SIGN_MULTISIG("sign_multisig <filename>");
const char* USAGE_SUBMIT_MULTISIG("submit_multisig <filename>");
const char* USAGE_EXPORT_RAW_MULTISIG_TX("export_raw_multisig_tx <filename>");
const char* USAGE_PRINT_RING("print_ring <key_image> | <txid>");
const char* USAGE_SET_RING("set_ring <key_image> absolute|relative <index> [<index>...] )");
const char* USAGE_SAVE_KNOWN_RINGS("save_known_rings");
const char* USAGE_MARK_OUTPUT_SPENT("mark_output_spent <amount>/<offset> | <filename> [add]");
const char* USAGE_MARK_OUTPUT_UNSPENT("mark_output_unspent <amount>/<offset>");
const char* USAGE_IS_OUTPUT_SPENT("is_output_spent <amount>/<offset>");
const char* USAGE_PUBLIC_NODES("public_nodes");
const char* USAGE_RPC_PAYMENT_INFO("rpc_payment_info");
const char* USAGE_START_MINING_FOR_RPC("start_mining_for_rpc");
const char* USAGE_STOP_MINING_FOR_RPC("stop_mining_for_rpc");
const char* USAGE_SHOW_QR_CODE("show_qr_code [<subaddress_index>]");
const char* USAGE_NET_STATS("net_stats");
const char* USAGE_WELCOME("welcome");
const char* USAGE_VERSION("version");
const char* USAGE_RESCAN_BC("rescan_bc [hard|soft|keep_ki] [start_height=0]");
const char* USAGE_HELP("help [<command>]");
}
