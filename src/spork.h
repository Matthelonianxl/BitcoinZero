// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORK_H
#define SPORK_H

#include "hash.h"
#include "net.h"
#include "utilstrencodings.h"
#include "util.h"

class CSporkMessage;

/*
    Don't ever reuse these IDs for other sporks
    - This would result in old clients getting confused about which spork is for what
*/
static const int SPORK_START                                             = 10001;
static const int SPORK_END                                               = 10020;

static const int SPORK_2_INSTANTSEND_ENABLED                             = 10001;
static const int SPORK_3_INSTANTSEND_BLOCK_FILTERING                     = 10002;
static const int SPORK_4_BAN_OLD                                         = 10003;
static const int SPORK_5_INSTANTSEND_MAX_VALUE                           = 10004;
static const int SPORK_6_RECONSIDER_BLOCKS                               = 10005;
static const int SPORK_7_FEE_CHECKS                                      = 10006;
static const int SPORK_8_PAY_NODES                                       = 10007;
static const int SPORK_9_ZERO_OFF                                        = 10008;
static const int SPORK_11_MIN_V                                          = 10011;
static const int SPORK_12_F_PAYMENT_START                                = 10012;
static const int SPORK_13_F_PAYMENT_ENFORCEMENT                          = 10013;
static const int SPORK_14_BZNODE_PAYMENT_START                           = 10014;
static const int SPORK_15_BZNODE_PAYMENT_ENFORCEMENT                     = 10015;
static const int SPORK_16_MIN_BZNODE                                     = 10016;
static const int SPORK_17_RECONSIDER_BLOCKS                              = 10017;
static const int SPORK_18_FIXX_MN                                        = 10018;
static const int SPORK_19_FIXX_VN                                        = 10019;
static const int SPORK_20_SIGMA                                          = 10020;


static const int64_t SPORK_2_INSTANTSEND_ENABLED_DEFAULT                 = 4070908800;   // OFF
static const int64_t SPORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT         = 0;            // ON
static const int64_t SPORK_4_BAN_OLD_DEFAULT                             = 4070908800;   // OFF
static const int64_t SPORK_5_INSTANTSEND_MAX_VALUE_DEFAULT               = 1000;         // 1000 BZX
static const int64_t SPORK_6_RECONSIDER_BLOCKS_DEFAULT                   = 0;            // 0 BLOCKS
static const int64_t SPORK_7_FEE_CHECKS_DEFAULT                          = 4070908800;   // OFF
static const int64_t SPORK_8_PAY_NODES_DEFAULT                           = 4070908800;   // OFF
static const int64_t SPORK_9_ZERO_OFF_DEFAULT                            = 4070908800;   // OFF
static const int64_t SPORK_11_MIN_V_DEFAULT                              = 4070908800;   // OFF
static const int64_t SPORK_12_F_PAYMENT_START_DEFAULT                    = 4070908800;   // OFF
static const int64_t SPORK_13_F_PAYMENT_ENFORCEMENT_DEFAULT              = 4070908800;   // OFF
static const int64_t SPORK_14_BZNODE_PAYMENT_START_DEFAULT               = 4070908800;   // OFF
static const int64_t SPORK_15_BZNODE_PAYMENT_ENFORCEMENT_DEFAULT         = 4070908800;   // OFF
static const int64_t SPORK_16_MIN_BZNODE_DEFAULT                         = 99025;        //
static const int64_t SPORK_17_RECONSIDER_BLOCKS_DEFAULT                  = 0;            // 0 BLOCKS
static const int64_t SPORK_18_FIXX_MN_DEFAULT                            = 2000;         // OFF
static const int64_t SPORK_19_FIXX_VN_DEFAULT                            = 4070908800;   // OFF
static const int64_t SPORK_20_SIGMA_DEFAULT                              = 4070908800;   // OFF

extern std::map<uint256, CSporkMessage> mapSporks;

//
// Spork classes
// Keep track of all of the network spork settings
//

class CSporkMessage
{
private:
    std::vector<unsigned char> vchSig;

public:
    int nSporkID;
    int64_t nValue;
    int64_t nTimeSigned;

    CSporkMessage(int nSporkID, int64_t nValue, int64_t nTimeSigned) :
        nSporkID(nSporkID),
        nValue(nValue),
        nTimeSigned(nTimeSigned)
        {}

    CSporkMessage() :
        nSporkID(0),
        nValue(0),
        nTimeSigned(0)
        {}


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nSporkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << nSporkID;
        ss << nValue;
        ss << nTimeSigned;
        return ss.GetHash();
    }

    bool Sign(std::string strSignKey);
    bool CheckSignature();
    void Relay();
};


class CSporkManager
{
private:
    std::vector<unsigned char> vchSig;
    std::string strMasterPrivKey;
    std::map<int, CSporkMessage> mapSporksActive;

public:

    CSporkManager() {}

    void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ExecuteSpork(int nSporkID, int nValue);
    bool UpdateSpork(int nSporkID, int64_t nValue);

    bool IsSporkActive(int nSporkID);
    int64_t GetSporkValue(int nSporkID);
    int GetSporkIDByName(std::string strName);
    std::string GetSporkNameByID(int nSporkID);

    bool SetPrivKey(std::string strPrivKey);
};

extern CSporkManager sporkManager;

#endif
