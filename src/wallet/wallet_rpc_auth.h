// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
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

#include <string_view>

#include "wallet_rpc_server_error_codes.h"

namespace tools
{
namespace wallet_rpc
{
  /// Methods denied under `--restricted-rpc`. Names match JSON-RPC method
  /// strings. Handlers call deny_if_restricted for defense-in-depth; this
  /// catalog enables tests and future central dispatch.
  inline bool method_requires_full_access(std::string_view method) noexcept
  {
    return method == "transfer"
        || method == "transfer_split"
        || method == "sign_transfer"
        || method == "describe_transfer"
        || method == "submit_transfer"
        || method == "sweep_dust"
        || method == "sweep_unmixable"
        || method == "sweep_all"
        || method == "sweep_single"
        || method == "relay_tx"
        || method == "store"
        || method == "query_key"
        || method == "rescan_blockchain"
        || method == "sign"
        || method == "verify"
        || method == "stop_wallet"
        || method == "set_tx_notes"
        || method == "set_attribute"
        || method == "get_attribute"
        || method == "get_tx_key"
        || method == "get_tx_proof"
        || method == "check_tx_proof"
        || method == "get_spend_proof"
        || method == "check_spend_proof"
        || method == "get_transfers"
        || method == "get_transfers_csv"
        || method == "get_transfer_by_txid"
        || method == "export_outputs"
        || method == "import_outputs"
        || method == "export_key_images"
        || method == "import_key_images"
        || method == "add_address_book"
        || method == "edit_address_book"
        || method == "delete_address_book"
        || method == "refresh"
        || method == "auto_refresh"
        || method == "rescan_spent"
        || method == "change_wallet_password"
        || method == "create_wallet"
        || method == "open_wallet"
        || method == "close_wallet"
        || method == "generate_from_keys"
        || method == "restore_deterministic_wallet"
        || method == "prepare_multisig"
        || method == "make_multisig"
        || method == "export_multisig_info"
        || method == "import_multisig_info"
        || method == "finalize_multisig"
        || method == "exchange_multisig_keys"
        || method == "sign_multisig"
        || method == "submit_multisig"
        || method == "set_daemon"
        || method == "set_log_level"
        || method == "set_log_categories"
        || method == "start_mining"
        || method == "stop_mining"
        || method == "stake"
        || method == "register_service_node"
        || method == "request_stake_unlock"
        || method == "can_request_stake_unlock";
  }

  inline bool allow_wallet_rpc_method(std::string_view method, bool restricted) noexcept
  {
    if (!restricted)
      return true;
    return !method_requires_full_access(method);
  }

  /// Shared denial used by wallet-rpc handlers under `--restricted-rpc`.
  template <typename Error>
  inline bool deny_if_restricted(bool restricted, Error &er,
                                 const char *message = "Command unavailable in restricted mode.")
  {
    if (!restricted)
      return false;
    er.code = WALLET_RPC_ERROR_CODE_DENIED;
    er.message = message;
    return true;
  }
}
}
