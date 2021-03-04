// Copyright (c)2021, The GNTL Network
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

//cryptonote_core strings
const std::string ascii_gntl_logo =
    "\n"
    "                                                            \n"
    "                               ,--.                 ,--.    \n"
    "            ,----..          ,--.'|        ,----,,---.'|    \n"
    "           /   /   \     ,--,:  : |      ,/   .`|;   : |    \n"
    "          |   :     : ,`--.'`|  ' :    ,`   .'  :|   | :    \n"
    "          .   |  ;. / |   :  :  | |  ;    ;     /:   : |    \n"
    "          .   ; /--`  :   |   \ | :.'___,/    ,' |   ' :    \n"
    "          ;   | ;  __ |   : '  '; ||    :     |  ;   ; '    \n"
    "          |   : |.' .''   ' ;.    ;;    |.';  ;  '   | |__  \n"
    "          .   | '_.' :|   | | \   |`----'  |  |  |   | :.'| \n"
    "          '   ; : \  |'   : |  ; .'    '   :  ;  '   :    ; \n"
    "          '   | '/  .'|   | '`--'      |   |  '  |   |  ./  \n"
    "          |   :    /  '   : |          '   :  |  ;   : ;    \n"
    "           \   \ .'   ;   |.'          ;   |.'   |   ,/     \n"
    "            `---`     '---'            '---'     '---'      \n"
    "                                                            ";

const std::string ascii_gntl_info =
    "\n"
    "**********************************************************************\n"
    "You can set the level of process detailization through \"set_log <level|categories>\"\n"
    "command, where <level> is between 0 (no details) and 4 (very verbose),\n"
    "or custom category based levels (eg, *:WARNING).\n"
    "Use the \"help\" command to see the list of available commands.\n"
    "Use \"help <command>\" to see a command's documentation.\n"
    "**********************************************************************";
const std::string main_message_true =
    "\n"
    "The daemon is running offline and will not attempt to sync to the GNTL-Net.\n";
const std::string main_message_false =
    "\n"
    "The daemon will start synchronizing with the network. This may take a long time to complete.\n";

//cryptonote_protocol_handler
const std::string crypto_synced =
    "**********************************************************************\n"
    "You are now synchronized with the network. You may now start gntl-wallet-cli.\n"
    "Use the \"help\" command to see the list of available commands.\n"
    "**********************************************************************";
