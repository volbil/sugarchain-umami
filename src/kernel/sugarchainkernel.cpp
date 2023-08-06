// Copyright (c) 2022 The Sugarchain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <functional>
#include <string>

// Define G_TRANSLATION_FUN symbol in libsugarchainkernel library so users of the
// library aren't required to export this symbol
extern const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;
