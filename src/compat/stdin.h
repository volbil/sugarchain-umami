// Copyright (c) 2018 The Sugarchain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SUGARCHAIN_COMPAT_STDIN_H
#define SUGARCHAIN_COMPAT_STDIN_H

struct NoechoInst {
    NoechoInst();
    ~NoechoInst();
};

#define NO_STDIN_ECHO() NoechoInst _no_echo

bool StdinTerminal();
bool StdinReady();

#endif // SUGARCHAIN_COMPAT_STDIN_H
