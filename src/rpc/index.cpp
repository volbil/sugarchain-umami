// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/context.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txdb.h>
#include <txmempool.h>
#include <univalue.h>
#include <validation.h>
#include <key_io.h>

#include <stdint.h>

#include <condition_variable>
#include <memory>
#include <mutex>

using node::NodeContext;

// Addressindex
static bool getIndexKey(const std::string& str, uint256& hashBytes, int& type)
{
    CTxDestination dest = DecodeDestination(str);
    if (!IsValidDestination(dest)) {
        type = 0;
        return false;
    }

    if (dest.index() == DI::_PKHash) {
        const PKHash &id = std::get<PKHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_PUBKEY_ADDRESS;
        return true;
    }
    if (dest.index() == DI::_ScriptHash) {
        const ScriptHash& id = std::get<ScriptHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_SCRIPT_ADDRESS;
        return true;
    }
    if (dest.index() == DI::_WitnessV0KeyHash) {
        const WitnessV0KeyHash& id = std::get<WitnessV0KeyHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_WITNESS_V0_KEYHASH;
        return true;
    }
    if (dest.index() == DI::_WitnessV0ScriptHash) {
        const WitnessV0ScriptHash& id = std::get<WitnessV0ScriptHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 32);
        type = ADDR_INDT_WITNESS_V0_SCRIPTHASH;
        return true;
    }
    if (dest.index() == DI::_WitnessV1Taproot) {
        const WitnessV1Taproot& id = std::get<WitnessV1Taproot>(dest);
        memcpy(hashBytes.begin(), id.begin(), 32);
        type = ADDR_INDT_WITNESS_V1_TAPROOT;
        return true;
    }
    type = ADDR_INDT_UNKNOWN;
    return false;
}

static bool getAddressesFromParams(const UniValue& params, std::vector<std::pair<uint256, int> > &addresses)
{
    if (params[0].isStr()) {
        uint256 hashBytes;
        int type = 0;
        if (!getIndexKey(params[0].get_str(), hashBytes, type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    } else if (params[0].isObject()) {

        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }

        std::vector<UniValue> values = addressValues.getValues();

        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {

            uint256 hashBytes;
            int type = 0;
            if (!getIndexKey(it->get_str(), hashBytes, type)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }
            addresses.push_back(std::make_pair(hashBytes, type));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    return true;
}

bool GetAddressIndex(ChainstateManager &chainman, const uint256 &addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex, int start = 0, int end = 0)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};

    if (!fAddressIndex) {
        return error("Address index not enabled");
    }

    if (!pblocktree->ReadAddressIndex(addressHash, type, addressIndex, start, end)) {
        return error("Unable to get txids for address");
    }

    return true;
};

static bool HashOnchainActive(ChainstateManager &chainman, const uint256 &hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlockIndex* pblockindex = chainman.m_blockman.LookupBlockIndex(hash);

    if (!chainman.ActiveChain().Contains(pblockindex)) {
        return false;
    }

    return true;
};

bool GetTimestampIndex(ChainstateManager &chainman, const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    if (!fTimestampIndex) {
        return error("Timestamp index not enabled");
    }
    if (!pblocktree->ReadTimestampIndex(high, low, hashes)) {
        return error("Unable to get hashes for timestamps");
    }

    if (fActiveOnly) {
        for (auto it = hashes.begin(); it != hashes.end(); ) {
            if (!HashOnchainActive(chainman, it->first)) {
                it = hashes.erase(it);
            } else {
                ++it;
            }
        }
    }

    return true;
};

static RPCHelpMan getaddressbalance()
{
    return RPCHelpMan{"getaddressbalance",
                "\nReturns the balance for an address(es) (requires addressindex to be enabled).\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The Bitcoin address "},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_AMOUNT, "balance", "The current balance in satoshis"},
                        {RPCResult::Type::STR_AMOUNT, "received", "The total number of satoshis received (including change)"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddressbalance", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressbalance", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::vector<std::pair<uint256, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 7");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex(chainman, (*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    int nHeight = (unsigned int) chainman.ActiveChain().Height();

    CAmount balance = 0;
    CAmount balance_spendable = 0;
    CAmount balance_immature = 0;
    CAmount received = 0;

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        if (it->second > 0) {
            received += it->second;
        }
        if (it->first.txindex == 0 && nHeight - it->first.blockHeight < COINBASE_MATURITY) {
            balance_immature += it->second;
        } else {
            balance_spendable += it->second;
        }
        balance += it->second;
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("balance", balance);
    result.pushKV("balance_immature", balance_immature);
    result.pushKV("balance_spendable", balance_spendable);
    result.pushKV("received", received);

    return result;
},
    };
}


static RPCHelpMan getaddresstxids()
{
    return RPCHelpMan{"getaddresstxids",
                "\nReturns the txids for an address(es) (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                    {"start", RPCArg::Type::NUM, RPCArg::Default{0}, "The start block height."},
                    {"end", RPCArg::Type::NUM, RPCArg::Default{0}, "The end block height."},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::STR_HEX, "transactionid", "The transaction txid"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    if (!fAddressIndex) {
        throw JSONRPCError(RPC_MISC_ERROR, "Address index is not enabled.");
    }
    ChainstateManager &chainman = EnsureAnyChainman(request.context);

    std::vector<std::pair<uint256, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    int start = 0;
    int end = 0;
    if (request.params[0].isObject()) {
        UniValue startValue = find_value(request.params[0].get_obj(), "start");
        UniValue endValue = find_value(request.params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.getInt<int>();
            end = endValue.getInt<int>();
        }
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    std::set<std::pair<int, std::string> > txids;
    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        int height = it->first.blockHeight;
        std::string txid = it->first.txhash.GetHex();

        if (addresses.size() > 1) {
            txids.insert(std::make_pair(height, txid));
        } else {
            if (txids.insert(std::make_pair(height, txid)).second) {
                result.push_back(txid);
            }
        }
    }

    if (addresses.size() > 1) {
        for (std::set<std::pair<int, std::string> >::const_iterator it=txids.begin(); it!=txids.end(); it++) {
            result.push_back(it->second);
        }
    }

    return result;
},
    };
}


static RPCHelpMan getblockhashes()
{
    return RPCHelpMan{"getblockhashes",
                "\nReturns array of hashes of blocks within the timestamp range provided.\n",
                {
                    {"high", RPCArg::Type::NUM, RPCArg::Optional::NO, "The newer block timestamp."},
                    {"low", RPCArg::Type::NUM, RPCArg::Optional::NO, "The older block timestamp."},
                    {"options", RPCArg::Type::OBJ, RPCArg::Default{UniValue::VOBJ}, "",
                        {
                            {"noOrphans", RPCArg::Type::BOOL, RPCArg::Default{false}, "Only include blocks on the main chain."},
                            {"logicalTimes", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include logical timestamps with hashes."},
                        },
                    },
                },
                RPCResults{
                    {RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::STR_HEX, "hash", "The block hash"},
                    }},
                    {RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::OBJ, "", "", {
                            {RPCResult::Type::STR_HEX, "blockhash", "The block hash"},
                            {RPCResult::Type::NUM, "logicalts", "The logical timestamp"},
                            {RPCResult::Type::NUM, "height", "The height of the block containing the spending tx"},
                        }}
                    }}
                },
                RPCExamples{
            HelpExampleCli("getblockhashes", "1231614698 1231024505 '{\"noOrphans\":false, \"logicalTimes\":true}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getblockhashes", "1231614698, 1231024505")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    unsigned int high = request.params[0].getInt<int>();
    unsigned int low = request.params[1].getInt<int>();
    bool fActiveOnly = false;
    bool fLogicalTS = false;

    if (request.params.size() > 2) {
        if (request.params[2].isObject()) {
            UniValue noOrphans = find_value(request.params[2].get_obj(), "noOrphans");
            UniValue returnLogical = find_value(request.params[2].get_obj(), "logicalTimes");

            if (noOrphans.isBool()) {
                fActiveOnly = noOrphans.get_bool();
            }
            if (returnLogical.isBool()) {
                fLogicalTS = returnLogical.get_bool();
            }
        }
    }

    std::vector<std::pair<uint256, unsigned int> > blockHashes;

    {
        LOCK(cs_main);
        if (!GetTimestampIndex(chainman, high, low, fActiveOnly, blockHashes)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for block hashes");
        }
    }

    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<uint256, unsigned int> >::const_iterator it=blockHashes.begin(); it!=blockHashes.end(); it++) {
        if (fLogicalTS) {
            UniValue item(UniValue::VOBJ);
            item.pushKV("blockhash", it->first.GetHex());
            item.pushKV("logicalts", int(it->second));
            result.push_back(item);
        } else {
            result.push_back(it->first.GetHex());
        }
    }

    return result;
},
    };
}

// getaddressdeltas
// getaddressutxos
// getaddressmempool
// getblockhashes
// getspentinfo

void RegisterIndexRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        // Address index
        {"getaddressbalance", &getaddressbalance},
        {"getaddresstxids",   &getaddresstxids},
        // {"getblockhashes",    &getblockhashes},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
