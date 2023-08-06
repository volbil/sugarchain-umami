// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Sugarchain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SUGARCHAIN_SCRIPT_SUGARCHAINCONSENSUS_H
#define SUGARCHAIN_SCRIPT_SUGARCHAINCONSENSUS_H

#include <stdint.h>

#if defined(BUILD_SUGARCHAIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include <config/sugarchain-config.h>
  #if defined(_WIN32)
    #if defined(HAVE_DLLEXPORT_ATTRIBUTE)
      #define EXPORT_SYMBOL __declspec(dllexport)
    #else
      #define EXPORT_SYMBOL
    #endif
  #elif defined(HAVE_DEFAULT_VISIBILITY_ATTRIBUTE)
    #define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBSUGARCHAINCONSENSUS)
  #define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
  #define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SUGARCHAINCONSENSUS_API_VER 1

typedef enum sugarchainconsensus_error_t
{
    sugarchainconsensus_ERR_OK = 0,
    sugarchainconsensus_ERR_TX_INDEX,
    sugarchainconsensus_ERR_TX_SIZE_MISMATCH,
    sugarchainconsensus_ERR_TX_DESERIALIZE,
    sugarchainconsensus_ERR_AMOUNT_REQUIRED,
    sugarchainconsensus_ERR_INVALID_FLAGS,
} sugarchainconsensus_error;

/** Script verification flags */
enum
{
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0), // evaluate P2SH (BIP16) subscripts
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2), // enforce strict DER (BIP66) compliance
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY           = (1U << 4), // enforce NULLDUMMY (BIP147)
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9), // enable CHECKLOCKTIMEVERIFY (BIP65)
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10), // enable CHECKSEQUENCEVERIFY (BIP112)
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_WITNESS             = (1U << 11), // enable WITNESS (BIP141)
    sugarchainconsensus_SCRIPT_FLAGS_VERIFY_ALL                 = sugarchainconsensus_SCRIPT_FLAGS_VERIFY_P2SH | sugarchainconsensus_SCRIPT_FLAGS_VERIFY_DERSIG |
                                                               sugarchainconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY | sugarchainconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
                                                               sugarchainconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY | sugarchainconsensus_SCRIPT_FLAGS_VERIFY_WITNESS
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not nullptr, err will contain an error/success code for the operation
EXPORT_SYMBOL int sugarchainconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                                 const unsigned char *txTo        , unsigned int txToLen,
                                                 unsigned int nIn, unsigned int flags, sugarchainconsensus_error* err);

EXPORT_SYMBOL int sugarchainconsensus_verify_script_with_amount(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, sugarchainconsensus_error* err);

EXPORT_SYMBOL unsigned int sugarchainconsensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // SUGARCHAIN_SCRIPT_SUGARCHAINCONSENSUS_H
