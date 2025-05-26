// Copyright (c)2020-2022, The Arqma Network
// Copyright (c)2020, ArqTras
// Portions of this software are available under BSD-3 license. Please see ORIGINAL-LICENSE for details
//
// All rights reserved.
//
// Authors and copyright holders give permission for following:
//
// 1. Redistribution and use in source and binary forms WITHOUT modification.
// 2. Modification of the source form for your own personal use.
//
// As long as the following conditions are met:
//
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
//
// Authors and copyright holders agree that:
//
// 8. This licence expires and the work covered by it is released into the
//    public domain on 1st of March 2021
//
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

namespace command_helper
{


  const char* USAGE_START_MINING("start_mining [<number_of_threads>] [bg_mining] [ignore_battery]");
  const char* USAGE_SET_DAEMON("set_daemon <host>[:<port>] [trusted|untrusted]");
  const char* USAGE_SHOW_BALANCE("balance [detail]");
  const char* USAGE_INCOMING_TRANSFERS("incoming_transfers [available|unavailable] [verbose] [uses] [index=<N1>[,<N2>[,...]]]");
  const char* USAGE_PAYMENTS("payments <PID_1> [<PID_2> ... <PID_N>]");
  const char* USAGE_PAYMENT_ID("payment_id");
  const char* USAGE_TRANSFER("transfer [index=<N1>[,<N2>,...]] [<priority>] (<URI> | <address> <amount>) [<payment_id>]");
  const char* USAGE_LOCKED_TRANSFER("locked_transfer [index=<N1>[,<N2>,...]] [<priority>] (<URI> | <addr> <amount>) <lockblocks> [<payment_id>]");
  const char* USAGE_LOCKED_SWEEP_ALL("locked_sweep_all [index=<N1>[,<N2>,...]] [<priority>] [<address>] <lockblocks> [<payment_id>]");
  const char* USAGE_SWEEP_ALL("sweep_all [index=<N1>[,<N2>,...]] [<priority>] [outputs=<N>] [<address>] [<payment_id>]");
  const char* USAGE_SWEEP_BELOW("sweep_below <amount_threshold> [index=<N1>[,<N2>,...]] [<priority>] <address> [<payment_id>]");
  const char* USAGE_SWEEP_SINGLE("sweep_single [<priority>] [outputs=<N>] <key_image> <address> [<payment_id>]");
  const char* USAGE_DONATE("donate [index=<N1>[,<N2>,...]] [<priority>] <amount> [<payment_id>]");
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
  const char* USAGE_SHOW_QR_CODE("show_qr_code [<subaddress_index>]");
  const char* USAGE_NET_STATS("net_stats");
  const char* USAGE_WELCOME("welcome");
  const char* USAGE_VERSION("version");
  const char* USAGE_RESCAN_BC("rescan_bc [hard|soft|keep_ki] [start_height=0]");
  const char* USAGE_HELP("help [<command>]");

//chars related to this one up

  const char* START_MINING("Start mining in the daemon (bg_mining and ignore_battery are optional booleans).");
  const char* STOP_MINING("Stop mining in the daemon.");
  const char* SET_DAEMON("Set another daemon to connect to.");
  const char* SAVE_BC("Save the current blockchain data.");
  const char* REFRESH("Synchronize the transactions and balance.");
  const char* BALANCE("Show the wallet's balance of the currently selected account.");
  const char* INCOMING_TRANSFERS("Show the incoming transfers, all or filtered by availability and address index.\n\n"
                                 "Output format:\n"
                                 "Amount, Spent(\"T\"|\"F\"), \"locked\"|\"unlocked\", RingCT, Global Index, Transaction Hash, Address Index, [Public Key, Key Image] ");
  const char* PAYMENTS("Show the payments for the given payment IDs.");
  const char* BC_HEIGHT("Show the blockchain height.");
  const char* TRANSFER("Transfer <amount> to <address>. If the parameter \"index=<N1>[,<N2>,...]\" is specified, the wallet uses outputs received by addresses of those indices. If omitted, the wallet randomly chooses address indices to be used. In any case, it tries its best not to combine outputs across multiple addresses. <priority> is the priority of the transaction. The higher the priority, the higher the transaction fee. Valid values in priority order (from lowest to highest) are: unimportant, normal, elevated, priority. If omitted, the default value (see the command \"set priority\") is used. <ring_size> is the number of inputs to include for untraceability. Multiple payments can be made at once by adding URI_2 or <address_2> <amount_2> etcetera (before the payment ID, if it's included)");
  const char* LOCKED_TRANSFER("Transfer <amount> to <address> and lock it for <lockblocks> (max. 1000000). If the parameter \"index=<N1>[,<N2>,...]\" is specified, the wallet uses outputs received by addresses of those indices. If omitted, the wallet randomly chooses address indices to be used. In any case, it tries its best not to combine outputs across multiple addresses. <priority> is the priority of the transaction. The higher the priority, the higher the transaction fee. Valid values in priority order (from lowest to highest) are: unimportant, normal, elevated, priority. If omitted, the default value (see the command \"set priority\") is used. <ring_size> is the number of inputs to include for untraceability. Multiple payments can be made at once by adding URI_2 or <address_2> <amount_2> etcetera (before the payment ID, if it's included)");
  const char* LOCKED_SWEEP_ALL("Send all unlocked balance to an address and lock it for <lockblocks> (max. 1000000). If no address is specified the address of the currently selected account will be used. If the parameter \"index<N1>[,<N2>,...]\" is specified, the wallet sweeps outputs received by those address indices. If omitted, the wallet randomly chooses an address index to be used. <priority> is the priority of the sweep. The higher the priority, the higher the transaction fee. Valid values in priority order (from lowest to highest) are: unimportant, normal, elevated, priority. If omitted, the default value (see the command \"set priority\") is used. <ring_size> is the number of inputs to include for untraceability.");
  const char* SWEEP_UNMIXABLE("Send all unmixable outputs to yourself with ring_size 1");
  const char* SWEEP_ALL("Send all unlocked balance to an address. If no address is specified the address of the currently selected account will be used. If the parameter \"index<N1>[,<N2>,...]\" is specified, the wallet sweeps outputs received by those address indices. If omitted, the wallet randomly chooses an address index to be used. If the parameter \"outputs=<N>\" is specified and  N > 0, wallet splits the transaction into N even outputs.");
  const char* SWEEP_BELOW("Send all unlocked outputs below the threshold to an address. If no address is specified the address of the currently selected account will be used.");
  const char* SWEEP_SINGLE("Send a single output of the given key image to an address without change.");
  const char* DONATE("Donate <amount> to the development team (donations.arqma.com).");
  const char* SIGN_TRANSFER("Sign a transaction from a file. If the parameter \"export_raw\" is specified, transaction raw hex data suitable for the daemon RPC /sendrawtransaction is exported.");
  const char* SUBMIT_TRANSFER("Submit a signed transaction from a file.");
  const char* SET_LOG("Change the current log detail (level must be <0-4>).");
  const char* ACCOUNT("If no arguments are specified, the wallet shows all the existing accounts along with their balances.\n"
                      "If the \"new\" argument is specified, the wallet creates a new account with its label initialized by the provided label text (which can be empty).\n"
                      "If the \"switch\" argument is specified, the wallet switches to the account specified by <index>.\n"
                      "If the \"label\" argument is specified, the wallet sets the label of the account specified by <index> to the provided label text.\n"
                      "If the \"tag\" argument is specified, a tag <tag_name> is assigned to the specified accounts <account_index_1>, <account_index_2>, ....\n"
                      "If the \"untag\" argument is specified, the tags assigned to the specified accounts <account_index_1>, <account_index_2> ..., are removed.\n"
                      "If the \"tag_description\" argument is specified, the tag <tag_name> is assigned an arbitrary text <description>.");
  const char* ADDRESS("If no arguments are specified or <index> is specified, the wallet shows the default or specified address. If \"all\" is specified, the wallet shows all the existing addresses in the currently selected account. If \"new \" is specified, the wallet creates a new address with the provided label text (which can be empty). If \"label\" is specified, the wallet sets the label of the address specified by <index> to the provided label text.");
  const char* INTEGRATED_ADDRESS("Encode a payment ID into an integrated address for the current wallet public address (no argument uses a random payment ID), or decode an integrated address to standard address and payment ID");
  const char* ADDRESS_BOOK("Print all entries in the address book, optionally adding/deleting an entry to/from it.");
  const char* SAVE("Save the wallet data.");
  const char* SAVE_WATCH_ONLY("Save a watch-only keys file.");
  const char* VIEWKEY("Display the private view key.");
  const char* SPENDKEY("Display the private spend key.");
  const char* SEED("Display the Electrum-style mnemonic seed");
  const char* SET("Available options:\n "
                  "seed language\n "
                  "  Set the wallet's seed language.\n "
                  "always-confirm-transfers <1|0>\n "
                  "  Whether to confirm unsplit txes.\n "
                  "print-ring-members <1|0>\n "
                  "  Whether to print detailed information about ring members during confirmation.\n "
                  "store-tx-info <1|0>\n "
                  "  Whether to store outgoing tx info (destination address, payment ID, tx secret key) for future reference.\n "
                  //"default-ring-size <n>\n "
                  //"  Set the default ring size (obsolete).\n "
                  "auto-refresh <1|0>\n "
                  "  Whether to automatically synchronize new blocks from the daemon.\n "
                  "refresh-type <full|optimize-coinbase|no-coinbase|default>\n "
                  "  Set the wallet's refresh behaviour.\n "
                  "priority [0|1|2|3|4]\n "
                  "  Set the fee to default/unimportant/normal/elevated/priority.\n "
                  "confirm-missing-payment-id <1|0>\n "
                  "ask-password <0|1|2   (or never|action|decrypt)>\n "
                  "unit <arqma|milliarq|microarq|nanoarq>\n "
                  "  Set the default Arqma (sub-)unit.\n "
                  "min-outputs-count [n]\n "
                  "  Try to keep at least that many outputs of value at least min-outputs-value.\n "
                  "min-outputs-value [n]\n "
                  "  Try to keep at least min-outputs-count outputs of at least that value.\n "
                  "merge-destinations <1|0>\n "
                  "  Whether to merge multiple payments to the same destination address.\n "
                  "confirm-backlog <1|0>\n "
                  "  Whether to warn if there is transaction backlog.\n "
                  "confirm-backlog-threshold [n]\n "
                  "  Set a threshold for confirm-backlog to only warn if the transaction backlog is greater than n blocks.\n "
                  "refresh-from-block-height [n]\n "
                  "  Set the height before which to ignore blocks.\n "
                  "auto-low-priority <1|0>\n "
                  "  Whether to automatically use the low priority fee level when it's safe to do so.\n "
                  "segregate-pre-fork-outputs <1|0>\n "
                  "  Set this if you intend to spend outputs on both ArQmA AND a key reusing fork.\n "
                  "key-reuse-mitigation2 <1|0>\n "
                  "  Set this if you are not sure whether you will spend on a key reusing ArQmA fork later.\n"
                  "subaddress-lookahead <major>:<minor>\n "
                  "  Set the lookahead sizes for the subaddress hash table.\n "
                  "  Set this if you are not sure whether you will spend on a key reusing ArQmA fork later.\n "
                  "segregation-height <n>\n "
                  "  Set to the height of a key reusing fork you want to use, 0 to use default.\n"
                  "export-format <\"binary\"|\"ascii\">\n "
                  "  Save all exported files as binary (cannot be copied and pasted) or ascii (can be).\n ");
  const char* ENCRYPTED_KEY("Display the encrypted Electrum-style mnemonic seed.");
  const char* RESCAN_SPENT("Rescan the blockchain for spent outputs.");
  const char* GET_TX_KEY("Get the transaction key (r) for a given <txid>.");
  const char* SET_TX_KEY("Set the transaction key (r) for a given <txid> in case the tx was made by some other device or 3rd party wallet.");
  const char* CHECK_TX_KEY("Check the amount going to <address> in <txid>.");
  const char* GET_TX_PROOF("Generate a signature proving funds sent to <address> in <txid>, optionally with a challenge string <message>, using either the transaction secret key (when <address> is not your wallet's address) or the view secret key (otherwise), which does not disclose the secret key.");
  const char* CHECK_TX_PROOF("Check the proof for funds going to <address> in <txid> with the challenge string <message> if any.");
  const char* GET_SPEND_PROOF("Generate a signature proving that you generated <txid> using the spend secret key, optionally with a challenge string <message>.");
  const char* CHECK_SPEND_PROOF("Check a signature proving that the signer generated <txid>, optionally with a challenge string <message>.");
  const char* GET_RESERVE_PROOF("Generate a signature proving that you own at least this much, optionally with a challenge string <message>.\n"
                                "If 'all' is specified, you prove the entire sum of all of your existing accounts' balances.\n"
                                "Otherwise, you prove the reserve of the smallest possible amount above <amount> available in your current account.");
  const char* CHECK_RESERVE_PROOF("Check a signature proving that the owner of <address> holds at least this much, optionally with a challenge string <message>.");
  const char* SHOW_TRANSFERS("Show the incoming/outgoing transfers within an optional height range.\n\n"
                             "Output format:\n"
                             "In or Coinbase:    Block Number, \"block\"|\"in\",              Time, Amount,  Transaction Hash, Payment ID, Subaddress Index,                     \"-\", Note\n"
                             "Out:               Block Number, \"out\",                     Time, Amount*, Transaction Hash, Payment ID, Fee, Destinations, Input addresses**, \"-\", Note\n"
                             "Pool:                            \"pool\", \"in\",              Time, Amount,  Transaction Hash, Payment Id, Subaddress Index,                     \"-\", Note, Double Spend Note\n"
                             "Pending or Failed:               \"failed\"|\"pending\", \"out\", Time, Amount*, Transaction Hash, Payment ID, Fee, Input addresses**,               \"-\", Note\n\n"
                             "* Excluding change and fee.\n"
                             "** Set of address indices used as inputs in this transfer.");
  const char* EXPORT_TRANSFERS("Export to CSV the incoming/outgoing transfers within an optional height range.");
  const char* UNSPENT_OUTPUTS("Show the unspent outputs of a specified address within an optional amount range.");
  const char* RESCAN_BC("Rescan the blockchain from scratch. If \"hard\" is specified, you will lose any information which can not be recovered from the blockchain itself.");
  const char* SET_TX_NOTE("Set an arbitrary string note for a <txid>.");
  const char* GET_TX_NOTE("Get a string note for a txid.");
  const char* SET_DESCRIPTION("Set an arbitrary description for the wallet.");
  const char* GET_DESCRIPTION("Get the description of the wallet.");
  const char* STATUS("Show the wallet's status.");
  const char* WALLET_INFO("Show the wallet's information.");
  const char* SIGN("Sign the contents of a file.");
  const char* VERIFY("Verify a signature on the contents of a file.");
  const char* EXPORT_KEY_IMAGES("Export a signed set of key images to a <filename>.");
  const char* IMPORT_KEY_IMAGES("Import a signed key images list and verify their spent status.");
  const char* HW_RECONNECT("Attempts to reconnect HW wallet.");
  const char* EXPORT_OUTPUTS("Export a set of outputs owned by this wallet.");
  const char* IMPORT_OUTPUTS("Import a set of outputs owned by this wallet.");
  const char* SHOW_TRANSFER("Show information about a transfer to/from this address.");
  const char* PASSWORD("Change the wallet's password.");
  const char* PAYMENT_ID("Generate a new random full size payment id. These will be unencrypted on the blockchain, see integrated_address for encrypted short payment ids.");
  const char* FEE("Print the information about the current fee and transaction backlog.");
  const char* PREPARE_MULTISIG("Export data needed to create a multisig wallet");
  const char* MAKE_MULTISIG("Turn this wallet into a multisig wallet");
  const char* FINALIZE_MULTISIG("Turn this wallet into a multisig wallet, extra step for N-1/N wallets");
  const char* EXCHANGE_MULTISIG_KEYS("Performs extra multisig keys exchange rounds. Needed for arbitrary M/N multisig wallets");
  const char* EXPORT_MULTISIG_INFO("Export multisig info for other participants");
  const char* IMPORT_MULTISIG_INFO("Import multisig info from other participants");
  const char* SIGN_MULTISIG("Sign a multisig transaction from a file");
  const char* SUBMIT_MULTISIG("Submit a signed multisig transaction from a file");
  const char* EXPORT_RAW_MULTISIG_TX("Export a signed multisig transaction to a file");
  const char* PRINT_RING("Print the ring(s) used to spend a given key image or transaction (if the ring size is > 1)\n\n"
                         "Output format:\n"
                         "Key Image, \"absolute\", list of rings");
  const char* SET_RING("Set the ring used for a given key image, so it can be reused in a fork");
  const char* SAVE_KNOWN_RINGS("Save known rings to the shared rings database");
  const char* MARK_OUTPUT_SPENT("Mark output(s) as spent so they never get selected as fake outputs in a ring");
  const char* MARK_OUTPUT_UNSPENT("Mark an output as unspent so it may get selected as a fake output in a ring");
  const char* IS_OUTPUT_SPENT("Checks whether an output is marked as spent");
  const char* NET_STATS("Prints simple network stats.");
  const char* WELCOME("Prints basic info about Arqma for first time users");
  const char* VERSION("Returns version information.");
  const char* SHOW_QR_CODE("Show address as QR code.");
  const char* HELP("Show the help section or the documentation about a <command>.");

  // Service-Nodes

  const char* USAGE_STAKE("stake [index=<N1>[,<N2>,...]] [<priority>] <service node pubkey> <amount|percent_%>");
  const char* USAGE_REGISTER_SERVICE_NODE("register_service_node [index=<N1>[,<N2>,...]] [<priority>] <operator cut> <address_1> <%-contribution_1> [<address2> <%-contribution_2> [...]] <expiration timestamp> <pubkey> <signature>");
  const char* USAGE_REQUEST_STAKE_UNLOCK("request_stake_unlock <service_node_pubkey>");
  const char* USAGE_PRINT_LOCKED_STAKES("print_locked_stakes");

  const char* STAKE("Send a transfer to this wallet's main account and lock it as a contribution stake to the given Service Node (which must be registered and awaiting contributions). The stake amount may be specified either as a fixed amount or as a percentage of the Service Node's total stake. The optional index= and <priority> parameters work as in the `transfer' command.");
  const char* REGISTER_SERVICE_NODE("Send <amount> to this wallet's main account and lock it as an Operator Stake for newly registered Service Node. This command is normally generated at the Service Node with \"prepare_registration\" command used. Optional parameters such \"index=\" and \"<priority>\" works in the same way like on \"transfer\" command.");
  const char* REQUEST_STAKE_UNLOCK("Request currently locked Stake in the given Service Node to be unlocked");
  const char* PRINT_LOCKED_STAKES("Print currently locked Stakes at the Service Node");
}
