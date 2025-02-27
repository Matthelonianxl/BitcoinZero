#ifndef MAIN_ZEROCOIN_H
#define MAIN_ZEROCOIN_H

#include "amount.h"
#include "chain.h"
#include "coins.h"
#include "consensus/validation.h"
#include "libzerocoin/Zerocoin.h"
#include "zerocoin_params.h"
#include <unordered_set>
#include <unordered_map>
#include <functional>

// Zerocoin transaction info, added to the CBlock to ensure zerocoin mint/spend transactions got their info stored into
// index
class CZerocoinTxInfo {
public:
    // all the zerocoin transactions encountered so far
    set<uint256> zcTransactions;
    // <denomination, pubCoin> for all the mints
    vector<pair<int,CBigNum> > mints;
    // serial for every spend (map from serial to denomination)
    map<CBigNum,int> spentSerials;

    // are there v1 spends in the block?
    bool fHasSpendV1;

    // information about transactions in the block is complete
    bool fInfoIsComplete;

    CZerocoinTxInfo(): fHasSpendV1(false), fInfoIsComplete(false) {}
    // finalize everything
    void Complete();
};

bool CheckZerocoinFoundersInputs(const CTransaction &tx, CValidationState &state, const Consensus::Params &params, int nHeight);

void DisconnectTipZC(CBlock &block, CBlockIndex *pindexDelete);

int ZerocoinGetNHeight(const CBlockHeader &block);

/*
 * State of minted/spent coins as extracted from the index
 */
class CZerocoinState {
public:
    // First and last block where mint (and hence accumulator update) with given denomination and id was seen
    struct CoinGroupInfo {
        CoinGroupInfo() : firstBlock(NULL), lastBlock(NULL), nCoins(0) {}

        // first and last blocks having coins with given denomination and id minted
        CBlockIndex *firstBlock;
        CBlockIndex *lastBlock;
        // total number of minted coins with such parameters
        int nCoins;
    };

private:
    // Custom hash for big numbers
    struct CBigNumHash {
        std::size_t operator()(const CBigNum &bn) const noexcept;
    };

    struct CMintedCoinInfo {
        int         denomination;
        int         id;
        int         nHeight;
    };

    // Collection of coin groups. Map from <denomination,id> to CoinGroupInfo structure
    map<pair<int, int>, CoinGroupInfo> coinGroups;
    // Set of all minted pubCoin values
    unordered_multimap<CBigNum,CMintedCoinInfo,CBigNumHash> mintedPubCoins;
    // Latest IDs of coins by denomination
    map<int, int> latestCoinIds;


public:
    CZerocoinState();

    // Set of all used coin serials. Allows multiple entries for the same coin serial for historical reasons
    unordered_multiset<CBigNum,CBigNumHash> usedCoinSerials;

    // serials of spends currently in the mempool mapped to tx hashes
    unordered_map<CBigNum,uint256,CBigNumHash> mempoolCoinSerials;

    // Add serial to the list of used ones
    void AddSpend(const CBigNum &serial);

    // Disconnect block from the chain rolling back mints and spends
    void RemoveBlock(CBlockIndex *index);

    // Query coin group with given denomination and id
    bool GetCoinGroupInfo(int denomination, int id, CoinGroupInfo &result);

    // Query if the coin serial was previously used
    bool IsUsedCoinSerial(const CBigNum &coinSerial);
    // Query if there is a coin with given pubCoin value
    bool HasCoin(const CBigNum &pubCoin);

    // Return height of mint transaction and id of minted coin
    int GetMintedCoinHeightAndId(const CBigNum &pubCoin, int denomination, int &id);

    // Reset to initial values
    void Reset();

    // Check if there is a conflicting tx in the blockchain or mempool
    bool CanAddSpendToMempool(const CBigNum &coinSerial);

    // Add spend into the mempool. Check if there is a coin with such serial in either blockchain or mempool
    bool AddSpendToMempool(const CBigNum &coinSerial, uint256 txHash);

    // Add spend(s) into the mempool. Check if there is a coin with such serial in either blockchain or mempool
    bool AddSpendToMempool(const vector<CBigNum> &coinSerials, uint256 txHash);

    // Get conflicting tx hash by coin serial number
    uint256 GetMempoolConflictingTxHash(const CBigNum &coinSerial);

    // Remove spend from the mempool (usually as the result of adding tx to the block)
    void RemoveSpendFromMempool(const CBigNum &coinSerial);

    static CZerocoinState *GetZerocoinState();
};

#endif
