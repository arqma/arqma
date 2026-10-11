// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include "arq_messaging/identity.hpp"

#include <string>

namespace arq_router {
/// HTTP handler for the `arqma-router` process (status, peel, store, onion forward).
std::string handle_http(const std::string& method, const std::string& path, const std::string& body,
                        const arq_messaging::Identity& hop, const std::string& storage_url = {},
                        const std::string& token = {});
} // namespace arq_router
