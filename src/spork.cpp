// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "darksend.h"
#include "main.h"
#include "spork.h"

#include <boost/lexical_cast.hpp>

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;

void CSporkManager::ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; // disable all Dash specific functionality

    if (strCommand == NetMsgType::SPORK) {

        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        uint256 hash = spork.GetHash();

        std::string strLogMsg;
        {
            LOCK(cs_main);
            pfrom->setAskFor.erase(hash);
            if(!chainActive.Tip()) return;
            strLogMsg = strprintf("SPORK -- hash: %s id: %d value: %10d bestHeight: %d peer=%d", hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Height(), pfrom->id);
        }

        if(mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                LogPrint("spork", "%s seen\n", strLogMsg);
                return;
            } else {
                LogPrintf("%s updated\n", strLogMsg);
            }
        } else {
            LogPrintf("%s new\n", strLogMsg);
        }

        if(!spork.CheckSignature()) {
            LogPrintf("CSporkManager::ProcessSpork -- invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        spork.Relay();

        //does a task if needed
        ExecuteSpork(spork.nSporkID, spork.nValue);

    } else if (strCommand == NetMsgType::GETSPORKS) {

        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        while(it != mapSporksActive.end()) {
            pfrom->PushMessage(NetMsgType::SPORK, it->second);
            it++;
        }
    }

}

void CSporkManager::ExecuteSpork(int nSporkID, int nValue)
{
    //correct fork via spork technology
    if(nSporkID == SPORK_17_RECONSIDER_BLOCKS && nValue > 0) {
        // allow to reprocess 24h of blocks max, which should be enough to resolve any issues
        int64_t nMaxBlocks = 576;
        // this potentially can be a heavy operation, so only allow this to be executed once per 10 minutes
        int64_t nTimeout = 10 * 60;

        static int64_t nTimeExecuted = 0; // i.e. it was never executed before

        if(GetTime() - nTimeExecuted < nTimeout) {
            LogPrint("spork", "CSporkManager::ExecuteSpork -- ERROR: Trying to reconsider blocks, too soon - %d/%d\n", GetTime() - nTimeExecuted, nTimeout);
            return;
        }

        if(nValue > nMaxBlocks) {
            LogPrintf("CSporkManager::ExecuteSpork -- ERROR: Trying to reconsider too many blocks %d/%d\n", nValue, nMaxBlocks);
            return;
        }


        LogPrintf("CSporkManager::ExecuteSpork -- Reconsider Last %d Blocks\n", nValue);

        ReprocessBlocks(nValue);
        nTimeExecuted = GetTime();
    }
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{

    CSporkMessage spork = CSporkMessage(nSporkID, nValue, GetTime());

    if(spork.Sign(strMasterPrivKey)) {
        spork.Relay();
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[nSporkID] = spork;
        return true;
    }

    return false;
}

// grab the spork, otherwise say it's off
bool CSporkManager::IsSporkActive(int nSporkID)
{
    int64_t r = -1;

    if(mapSporksActive.count(nSporkID)){
        r = mapSporksActive[nSporkID].nValue;
    } else {
        switch (nSporkID) {
            case SPORK_2_INSTANTSEND_ENABLED:                r = SPORK_2_INSTANTSEND_ENABLED_DEFAULT; break;
            case SPORK_3_INSTANTSEND_BLOCK_FILTERING:        r = SPORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT; break;
            case SPORK_4_BAN_OLD:                            r = SPORK_4_BAN_OLD_DEFAULT; break;
            case SPORK_5_INSTANTSEND_MAX_VALUE:              r = SPORK_5_INSTANTSEND_MAX_VALUE_DEFAULT; break;
            case SPORK_6_RECONSIDER_BLOCKS:                  r = SPORK_6_RECONSIDER_BLOCKS_DEFAULT; break;
            case SPORK_7_FEE_CHECKS:                         r = SPORK_7_FEE_CHECKS_DEFAULT; break;
            case SPORK_8_PAY_NODES:                          r = SPORK_8_PAY_NODES_DEFAULT; break;
            case SPORK_9_ZERO_OFF:                           r = SPORK_9_ZERO_OFF_DEFAULT; break;
            case SPORK_11_MIN_V:                             r = SPORK_11_MIN_V_DEFAULT; break;
            case SPORK_12_F_PAYMENT_START:                   r = SPORK_12_F_PAYMENT_START_DEFAULT; break;
            case SPORK_13_F_PAYMENT_ENFORCEMENT:             r = SPORK_13_F_PAYMENT_ENFORCEMENT_DEFAULT; break;
            case SPORK_14_BZNODE_PAYMENT_START:              r = SPORK_14_BZNODE_PAYMENT_START_DEFAULT; break;
            case SPORK_15_BZNODE_PAYMENT_ENFORCEMENT:        r = SPORK_15_BZNODE_PAYMENT_ENFORCEMENT_DEFAULT; break;
            case SPORK_16_MIN_BZNODE:                        r = SPORK_16_MIN_BZNODE_DEFAULT; break;
            case SPORK_17_RECONSIDER_BLOCKS:                 r = SPORK_17_RECONSIDER_BLOCKS_DEFAULT; break;
            case SPORK_18_FIXX_MN:                           r = SPORK_18_FIXX_MN_DEFAULT; break;
            case SPORK_19_FIXX_VN:                           r = SPORK_19_FIXX_VN_DEFAULT; break;
            case SPORK_20_SIGMA:                             r = SPORK_20_SIGMA; break;
            default:
                LogPrint("spork", "CSporkManager::IsSporkActive -- Unknown Spork ID %d\n", nSporkID);
                r = 4070908800ULL; // 2099-1-1 i.e. off by default
                break;
        }
    }

    return r < GetTime();
}

// grab the value of the spork on the network, or the default
int64_t CSporkManager::GetSporkValue(int nSporkID)
{
    if (mapSporksActive.count(nSporkID))
        return mapSporksActive[nSporkID].nValue;

    switch (nSporkID) {
        case SPORK_2_INSTANTSEND_ENABLED:                return SPORK_2_INSTANTSEND_ENABLED_DEFAULT;
        case SPORK_3_INSTANTSEND_BLOCK_FILTERING:        return SPORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT;
        case SPORK_4_BAN_OLD:                            return SPORK_4_BAN_OLD_DEFAULT;
        case SPORK_5_INSTANTSEND_MAX_VALUE:              return SPORK_5_INSTANTSEND_MAX_VALUE_DEFAULT;
        case SPORK_6_RECONSIDER_BLOCKS:                  return SPORK_6_RECONSIDER_BLOCKS_DEFAULT;
        case SPORK_7_FEE_CHECKS:                         return SPORK_7_FEE_CHECKS_DEFAULT;
        case SPORK_8_PAY_NODES:                          return SPORK_8_PAY_NODES_DEFAULT;
        case SPORK_9_ZERO_OFF:                           return SPORK_9_ZERO_OFF_DEFAULT;
        case SPORK_11_MIN_V:                             return SPORK_11_MIN_V_DEFAULT;
        case SPORK_12_F_PAYMENT_START:                   return SPORK_12_F_PAYMENT_START_DEFAULT;
        case SPORK_13_F_PAYMENT_ENFORCEMENT:             return SPORK_13_F_PAYMENT_ENFORCEMENT_DEFAULT;
        case SPORK_14_BZNODE_PAYMENT_START:              return SPORK_14_BZNODE_PAYMENT_START_DEFAULT;
        case SPORK_15_BZNODE_PAYMENT_ENFORCEMENT:        return SPORK_15_BZNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        case SPORK_16_MIN_BZNODE:                        return SPORK_16_MIN_BZNODE_DEFAULT;
        case SPORK_17_RECONSIDER_BLOCKS:                 return SPORK_17_RECONSIDER_BLOCKS_DEFAULT;
        case SPORK_18_FIXX_MN:                           return SPORK_18_FIXX_MN_DEFAULT;
        case SPORK_19_FIXX_VN:                           return SPORK_19_FIXX_VN_DEFAULT;
        case SPORK_20_SIGMA:                             return SPORK_20_SIGMA_DEFAULT;
        default:
            LogPrint("spork", "CSporkManager::GetSporkValue -- Unknown Spork ID %d\n", nSporkID);
            return -1;
    }

}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_2_INSTANTSEND_ENABLED")            return SPORK_2_INSTANTSEND_ENABLED;
    if (strName == "SPORK_3_INSTANTSEND_BLOCK_FILTERING")    return SPORK_3_INSTANTSEND_BLOCK_FILTERING;
    if (strName == "SPORK_4_BAN_OLD")                        return SPORK_4_BAN_OLD;
    if (strName == "SPORK_5_INSTANTSEND_MAX_VALUE")          return SPORK_5_INSTANTSEND_MAX_VALUE;
    if (strName == "SPORK_6_RECONSIDER_BLOCKS")              return SPORK_6_RECONSIDER_BLOCKS;
    if (strName == "SPORK_7_FEE_CHECKS")                     return SPORK_7_FEE_CHECKS;
    if (strName == "SPORK_8_PAY_NODES")                      return SPORK_8_PAY_NODES;
    if (strName == "SPORK_9_ZERO_OFF")                       return SPORK_9_ZERO_OFF;
    if (strName == "SPORK_11_MIN_V")                         return SPORK_11_MIN_V;
    if (strName == "SPORK_12_F_PAYMENT_START")               return SPORK_12_F_PAYMENT_START;
    if (strName == "SPORK_13_F_PAYMENT_ENFORCEMENT")         return SPORK_13_F_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_14_BZNODE_PAYMENT_START")          return SPORK_14_BZNODE_PAYMENT_START;
    if (strName == "SPORK_15_BZNODE_PAYMENT_ENFORCEMENT")    return SPORK_15_BZNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_16_MIN_BZNODE")                    return SPORK_16_MIN_BZNODE;
    if (strName == "SPORK_17_RECONSIDER_BLOCKS")             return SPORK_17_RECONSIDER_BLOCKS;
    if (strName == "SPORK_18_FIXX_MN")                       return SPORK_18_FIXX_MN;
    if (strName == "SPORK_19_FIXX_VN")                       return SPORK_19_FIXX_VN;
    if (strName == "SPORK_20_SIGMA")                         return SPORK_20_SIGMA;
    LogPrint("spork", "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
    return -1;
}

std::string CSporkManager::GetSporkNameByID(int nSporkID)
{
    switch (nSporkID) {
        case SPORK_2_INSTANTSEND_ENABLED:                return "SPORK_2_INSTANTSEND_ENABLED";
        case SPORK_3_INSTANTSEND_BLOCK_FILTERING:        return "SPORK_3_INSTANTSEND_BLOCK_FILTERING";
        case SPORK_4_BAN_OLD:                            return "SPORK_4_BAN_OLD";
        case SPORK_5_INSTANTSEND_MAX_VALUE:              return "SPORK_5_INSTANTSEND_MAX_VALUE";
        case SPORK_6_RECONSIDER_BLOCKS:                  return "SPORK_6_RECONSIDER_BLOCKS";
        case SPORK_7_FEE_CHECKS:                         return "SPORK_7_FEE_CHECKS";
        case SPORK_8_PAY_NODES:                          return "SPORK_8_PAY_NODES";
        case SPORK_9_ZERO_OFF:                           return "SPORK_9_ZERO_OFF";
        case SPORK_11_MIN_V:                             return "SPORK_11_MIN_V";
        case SPORK_12_F_PAYMENT_START:                   return "SPORK_12_F_PAYMENT_START";
        case SPORK_13_F_PAYMENT_ENFORCEMENT:             return "SPORK_13_F_PAYMENT_ENFORCEMENT";
        case SPORK_14_BZNODE_PAYMENT_START:              return "SPORK_14_BZNODE_PAYMENT_START";
        case SPORK_15_BZNODE_PAYMENT_ENFORCEMENT:        return "SPORK_15_BZNODE_PAYMENT_ENFORCEMENT";
        case SPORK_16_MIN_BZNODE:                        return "SPORK_16_MIN_BZNODE";
        case SPORK_17_RECONSIDER_BLOCKS:                 return "SPORK_17_RECONSIDER_BLOCKS";
        case SPORK_18_FIXX_MN:                           return "SPORK_18_FIXX_MN";
        case SPORK_19_FIXX_VN:                           return "SPORK_19_FIXX_VN";
        case SPORK_20_SIGMA:                             return "SPORK_20_SIGMA";
        default:
            LogPrint("spork", "CSporkManager::GetSporkNameByID -- Unknown Spork ID %d\n", nSporkID);
            return "Unknown";
    }
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage spork;

    spork.Sign(strPrivKey);

    if(spork.CheckSignature()){
        // Test signing successful, proceed
        LogPrintf("CSporkManager::SetPrivKey -- Successfully initialized as spork signer\n");
        strMasterPrivKey = strPrivKey;
        return true;
    } else {
        return false;
    }
}

bool CSporkMessage::Sign(std::string strSignKey)
{
    CKey key;
    CPubKey pubkey;
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nSporkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);

    if(!darkSendSigner.GetKeysFromSecret(strSignKey, key, pubkey)) {
        LogPrintf("CSporkMessage::Sign -- GetKeysFromSecret() failed, invalid spork key %s\n", strSignKey);
        return false;
    }

    if(!darkSendSigner.SignMessage(strMessage, vchSig, key)) {
        LogPrintf("CSporkMessage::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, strError)) {
        LogPrintf("CSporkMessage::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CSporkMessage::CheckSignature()
{
    //note: need to investigate why this is failing
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nSporkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);
    CPubKey pubkey(ParseHex(Params().SporkPubKey()));

    if(!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, strError)) {
        LogPrintf("CSporkMessage::CheckSignature -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

void CSporkMessage::Relay()
{
    CInv inv(MSG_SPORK, GetHash());
    RelayInv(inv);
}
