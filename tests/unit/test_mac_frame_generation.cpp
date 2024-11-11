/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "mac/mac.hpp"
#include "mac/mac_frame.hpp"
#include "radio/radio.hpp"

#include "test_platform.h"
#include "test_util.hpp"

namespace ot {

// const uint8_t kDstAddress[sizeof(Mac::ExtAddress)] = {0x01, 0xfe, 0xca, 0xef, 0xbe, 0x00, 0xad, 0xde};
// const uint8_t kSrcAddress[sizeof(Mac::ExtAddress)] = {0x02, 0xfe, 0xca, 0xef, 0xbe, 0x00, 0xad, 0xde};
const uint8_t kDstAddress[sizeof(Mac::ExtAddress)] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
const uint8_t kSrcAddress[sizeof(Mac::ExtAddress)] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static const Mac::ShortAddress sDstShortAddress = 0xaaaa;
Mac::ExtAddress                sDstExtAddress;
static const Mac::ShortAddress sSrcShortAddress = 0xbbbb;
Mac::ExtAddress                sSrcExtAddress;
static const Mac::PanId        sDstPanId = 0xdddd;
static const Mac::PanId        sSrcPanId = 0xeeee;

static Mac::Address sAddressDstExt;
static Mac::Address sAddressSrcExt;
static Mac::Address sAddressDstShort;
static Mac::Address sAssressSrcShort;

static uint8_t                  sPsdu[OT_RADIO_FRAME_MAX_SIZE];
static constexpr uint16_t       kInfoStringSize = 512; ///< Max chars for `InfoString` (ToInfoString()).
typedef String<kInfoStringSize> InfoString;

typedef struct ot2015FrameSecurityConfig
{
    Mac::KeyMaterial       mKey;
    uint8_t                mKeySequence;
    uint8_t                mFrameCounter;
    const Mac::ExtAddress *mExtAddress;
} ot2015FrameSecurityConfig;

typedef struct ot2015FrameConfig
{
    Mac::Address *mDestAddress;
    Mac::Address *mSrcAddress;

    bool mIsDstPanIdPresent;
    bool mIsSrcPanIdPresent;
    bool mIsCslIePresent;
    bool mIsSequenceSuppressed;
    bool mIsHeaderUpdated;

    Mac::Frame::SecurityLevel mSecurityLevel;
    Mac::Frame::KeyIdMode     mKeyIdMode;

    ot2015FrameSecurityConfig *mSecurityConfig;

    uint8_t mPsduLength;
} ot2015FrameConfig;

void SecurityConfigToString(ot2015FrameSecurityConfig &aConfig, char *aBuf, uint16_t aLength)
{
    char         buf[64] = {0};
    StringWriter writer(buf, sizeof(buf));

    writer.AppendHexBytes(aConfig.mKey.mKeyMaterial.mKey.m8, sizeof(aConfig.mKey.mKeyMaterial.mKey.m8));
    snprintf(aBuf, aLength, "keySeq=%u,FrameCounter=%u,ExtAddress:%s,Key:%s", aConfig.mKeySequence,
             aConfig.mFrameCounter, aConfig.mExtAddress->ToString().AsCString(), buf);
}

InfoString FrameToInfoString(Mac::TxFrame &aFrame, ot2015FrameConfig &aConfig)
{
    InfoString   string;
    uint8_t      commandId, type;
    Mac::Address src, dst;
    Mac::PanId   sSrcPanId, sDstPanId;
    uint32_t     frameCounter;
    bool         sequencePresent;
    uint16_t     version;
    bool         ieAppended = false;

    sequencePresent = aFrame.IsSequencePresent();

    string.Append("ver:");
    version = aFrame.GetVersion();
    if (version == Mac::Frame::kVersion2003)
    {
        string.Append("2003,");
    }
    else if (version == Mac::Frame::kVersion2006)
    {
        string.Append("2006,");
    }
    else if (version == Mac::Frame::kVersion2015)
    {
        string.Append("2015,");
    }
    else
    {
        string.Append("20xx,");
    }

    type = aFrame.GetType();

    switch (type)
    {
    case Mac::Frame::kTypeBeacon:
        string.Append("Bcon");
        break;

    case Mac::Frame::kTypeData:
        string.Append("Data");
        break;

    case Mac::Frame::kTypeAck:
        string.Append("Ack");
        break;

    case Mac::Frame::kTypeMacCmd:
        string.Append("Cmd");
        break;

#if OPENTHREAD_CONFIG_MAC_MULTIPURPOSE_FRAME
    case Mac::Frame::kTypeMultipurpose:
        string.Append("MP");
        break;
#endif

    default:
        string.Append("%d", type);
        break;
    }

    string.Append(",%s,", sequencePresent ? "seq" : "noseq");

    IgnoreError(aFrame.GetSrcAddr(src));
    IgnoreError(aFrame.GetDstAddr(dst));

    IgnoreError(aFrame.GetSrcPanId(sSrcPanId));
    IgnoreError(aFrame.GetDstPanId(sDstPanId));

    string.Append("dst[addr:");
    if (dst.GetType() == Mac::Address::kTypeShort)
    {
        string.Append("short");
    }
    else if (dst.GetType() == Mac::Address::kTypeExtended)
    {
        string.Append("extd");
    }
    else
    {
        string.Append("no");
    }

    string.Append(",pan:%s],", aFrame.IsDstPanIdPresent() ? "id" : "no");

    string.Append("src[addr:");
    if (src.GetType() == Mac::Address::kTypeShort)
    {
        string.Append("short");
    }
    else if (src.GetType() == Mac::Address::kTypeExtended)
    {
        string.Append("extd");
    }
    else
    {
        string.Append("no");
    }

    string.Append(",pan:%s],", aFrame.IsSrcPanIdPresent() ? "id" : "no");

    {
        if (aFrame.GetSecurityEnabled())
        {
            uint8_t level;
            uint8_t keyIdMode;
            char    buf[128];

            aFrame.GetSecurityLevel(level);
            aFrame.GetKeyIdMode(keyIdMode);

            string.Append("sec:[");
            string.Append("SecLevel:%u,keyIdMode:%u,IsHeaderUpdated:%u,", level, keyIdMode >> 3,
                          aConfig.mIsHeaderUpdated);
            if (aConfig.mSecurityConfig != nullptr && !aConfig.mIsHeaderUpdated)
            {
                SecurityConfigToString(*aConfig.mSecurityConfig, buf, sizeof(buf));
                string.Append("%s", buf);
            }
            string.Append("],");
        }
        else
        {
            string.Append("sec:no,");
        }
    }

    {
        const Mac::TimeIe           *timeIe       = aFrame.GetTimeIe();
        const Mac::CslIe            *cslIe        = aFrame.GetCslIe();
        const Mac::RendezvousTimeIe *rendezvousIe = aFrame.GetRendezvousTimeIe();
        const Mac::ConnectionIe     *connectionIe = aFrame.GetConnectionIe();

        if ((timeIe == nullptr) && (cslIe == nullptr) && (rendezvousIe == nullptr) && (connectionIe == nullptr))
        {
            string.Append("ie:no");
        }
        else
        {
            string.Append("ie[");

            if (timeIe != nullptr)
            {
                string.Append("time");
            }

            if (cslIe != nullptr)
            {
                string.Append("csl ");
            }

            if (rendezvousIe != nullptr)
            {
                string.Append("ren ");
            }

            if (connectionIe != nullptr)
            {
                string.Append("con ");
            }

            string.Append("]");
        }
    }

    if (type == Mac::Frame::kTypeMacCmd)
    {
        if (aFrame.GetCommandId(commandId) != kErrorNone)
        {
            commandId = 0xff;
        }

        string.Append(",");
        switch (commandId)
        {
        case Mac::Frame::kMacCmdDataRequest:
            string.Append("DataReq");
            break;

        case Mac::Frame::kMacCmdBeaconRequest:
            string.Append("BeaconReq");
            break;

        default:
            string.Append("Cmd(%d)", commandId);
            break;
        }
    }

    string.Append(",plen:%u", aFrame.GetPayloadLength());

    return string;
}

void OutputFrame(Mac::TxFrame &aFrame, ot2015FrameConfig &aConfig)
{
    char         buf[512] = {0};
    StringWriter writer(buf, sizeof(buf));

    writer.AppendHexBytes(aFrame.GetPsdu(), aFrame.GetLength());
    printf("name: %s\r\n", FrameToInfoString(aFrame, aConfig).AsCString());
    printf("psdu: %s\r\n", buf);
}

//-------------------------------------------------------------------------------------//

/*
 * Dataset:
 * 0e0800000000000100004a0300000e35060004001fffe002080bce12e0739cae590708fd892acf4510b8e7030f4f70656e5468726561642d3962356301029b5c0410da539bf20e7794e1f7c87a7f0f56e9dd0c0402a0f7f80003000014051000112233445566778899aabbccddcafe
 * Src Address: dead00beefcafe02
 * Key Sequence: 0
 * Key : 36e0a2195d8e4b8260ad0ccd8a399d4c
 */

uint8_t kKey[] = {0x36, 0xe0, 0xa2, 0x19, 0x5d, 0x8e, 0x4b, 0x82, 0x60, 0xad, 0x0c, 0xcd, 0x8a, 0x39, 0x9d, 0x4c};
ot2015FrameSecurityConfig sSecurityConfig;

void InitFrameInfo(Mac::TxFrame &aFrame)
{
    aFrame.mPsdu      = sPsdu;
    aFrame.mLength    = 0;
    aFrame.mRadioType = 0;

    sDstExtAddress.Set(kDstAddress, Mac::ExtAddress::kReverseByteOrder);
    sSrcExtAddress.Set(kSrcAddress, Mac::ExtAddress::kReverseByteOrder);

    sAddressDstExt.SetExtended(sDstExtAddress);
    sAddressSrcExt.SetExtended(sSrcExtAddress);

    sAddressDstShort.SetShort(sDstShortAddress);
    sAssressSrcShort.SetShort(sSrcShortAddress);

    memcpy(sSecurityConfig.mKey.mKeyMaterial.mKey.m8, kKey, sizeof(kKey));
    sSecurityConfig.mKeySequence  = 0;
    sSecurityConfig.mFrameCounter = 0;
    sSecurityConfig.mExtAddress   = &sSrcExtAddress;
}

/*
# Use the OT simulation to check the wakeup frame encryption:
$ ./script/cmake-build simulation
$ ./build/simulation/examples/apps/cli/ot-cli-ftd 1
> factoryreset
> dataset set active
0e0800000000000100004a0300000e35060004001fffe002080bce12e0739cae590708fd892acf4510b8e7030f4f70656e5468726561642d3962356301029b5c0410da539bf20e7794e1f7c87a7f0f56e9dd0c0402a0f7f80003000014051000112233445566778899aabbccddcafe
SetMacKey: mKeyId=1, mKeyType=0, mPrevMacFrameCounter=0, mMacFrameCounter=0, currKey=36e0a2195d8e4b8260ad0ccd8a399d4c
Done
> extaddr 0807060504030201
Done
> diag start
Done
>  diag frame fd87dddd1020304050607080010203040506070815000000000000000000820ee80305009bb8ea011c000000000000
Done
> diag send 1
PlatfTransmit: mTxDelay:0, mMaxCsmaBackoffs:0, mMaxFrameRetries:0, mRxChannelAfterTxDone:20, mTxPower:127,
mIsHeaderUpdated:0, mCsmaCaEnabled:0, mIsSecurityProcessed=0, mMacFrameCounter=0 len:47,
psdu:fd87dddd1020304050607080010203040506070815000000000000000000820ee80305009bb8ea011c000000000000 TxSec: keyId=1,
frameCounter=0, key=36e0a2195d8e4b8260ad0ccd8a399d4c


RadioTransmit: mTxDelay:0, mMaxCsmaBackoffs:0, mMaxFrameRetries:0, mRxChannelAfterTxDone:20, mTxPower:127,
mIsHeaderUpdated:1, mCsmaCaEnabled:0, mIsSecurityProcessed=1, mMacFrameCounter=1 len:47,
psdu:fd87dddd1020304050607080010203040506070815000000000000000001820ee80305009bb8ea011cec4f5b5e768c Done

# Run the unittest to generate the encrypted wakeup frame.
Wakeup frames ------------------------------------
name:
ver:2003,MP,noseq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:[SecLevel:5,keyIdMode:2,IsHeaderUpdated:1,],ie[ren
con], plen:0 psdu: fd87dddd1020304050607080010203040506070815000000000000000000820ee80305009bb8ea011c000000000000 name:

ver:2003,MP,noseq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:[SecLevel:5,keyIdMode:2,IsHeaderUpdated:0,keySeq=0,FrameCounter=0,ExtAddress:0807060504030201,Key:36e0a2195d8e4b8260ad0ccd8a399d4c],ie[ren
con], plen:0 psdu: fd87dddd1020304050607080010203040506070815000000000000000001820ee80305009bb8ea011cec4f5b5e0000
*/
void TestGenerateWakeupFrames()
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;
    ot2015FrameConfig  frames[] = {
         {&sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/,
          true /*SeqSuppressed*/, true /*isHeaderUpdated*/, Mac::Frame::SecurityLevel::kSecurityEncMic32,
          Mac::Frame::KeyIdMode::kKeyIdMode2, &sSecurityConfig, 0},

         {&sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/,
          true /*SeqSuppressed*/, false /*isHeaderUpdated*/, Mac::Frame::SecurityLevel::kSecurityEncMic32,
          Mac::Frame::KeyIdMode::kKeyIdMode2, &sSecurityConfig, 0}};

    Mac::ConnectionIe *connectionIe;

    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(frames); i++)
    {
        memset(sPsdu, 0, sizeof(sPsdu));
        frameInfo.Clear();

        frame.GenerateWakeupFrame(sDstPanId, sAddressDstExt, sAddressSrcExt);
        frame.GetRendezvousTimeIe()->SetRendezvousTime(1000);

        connectionIe = frame.GetConnectionIe();
        connectionIe->SetRetryInterval(1);
        connectionIe->SetRetryCount(12);

        frame.SetIsHeaderUpdated(frames[i].mIsHeaderUpdated);
        if ((frames[i].mSecurityLevel != Mac::Frame::SecurityLevel::kSecurityNone) && (!frame.IsHeaderUpdated()))
        {
            frame.SetAesKey(frames[i].mSecurityConfig->mKey);
            frame.SetFrameCounter(frames[i].mSecurityConfig->mFrameCounter);
            frame.SetKeyId((frames[i].mSecurityConfig->mKeySequence & 0x7f) + 1);
            frame.ProcessTransmitAesCcm(*frames[i].mSecurityConfig->mExtAddress);
        }

        OutputFrame(frame, frames[i]);
    }
}

void TestGenerate2015Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    ot2015FrameConfig frames[] = {
        {&sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/,
         false /*SeqSuppressed*/, false /*isHeaderUpdated*/, Mac::Frame::SecurityLevel::kSecurityNone,
         Mac::Frame::KeyIdMode::kKeyIdMode0, nullptr, 10},
        {&sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/,
         false /*SeqSuppressed*/, true /*isHeaderUpdated*/, Mac::Frame::SecurityLevel::kSecurityEncMic32,
         Mac::Frame::KeyIdMode::kKeyIdMode1, &sSecurityConfig, 10},
        {&sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/,
         false /*SeqSuppressed*/, false /*isHeaderUpdated*/, Mac::Frame::SecurityLevel::kSecurityEncMic32,
         Mac::Frame::KeyIdMode::kKeyIdMode1, &sSecurityConfig, 10}};

    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(frames); i++)
    {
        memset(sPsdu, 0, sizeof(sPsdu));
        frameInfo.Clear();

        frameInfo.mType               = Mac::Frame::kTypeData;
        frameInfo.mVersion            = Mac::Frame::kVersion2015;
        frameInfo.mAddrs.mDestination = *frames[i].mDestAddress;
        frameInfo.mAddrs.mSource      = *frames[i].mSrcAddress;
        frameInfo.mEmptyPayload       = true;

        frameInfo.mPanIds.Clear();
        if (frames[i].mIsDstPanIdPresent)
        {
            frameInfo.mPanIds.SetDestination(sDstPanId);
        }

        if (frames[i].mIsSrcPanIdPresent)
        {
            frameInfo.mPanIds.SetSource(sSrcPanId);
        }

        frameInfo.mAppendCslIe      = frames[i].mIsCslIePresent;
        frameInfo.mSuppressSequence = frames[i].mIsSequenceSuppressed;

        if (frames[i].mSecurityLevel != Mac::Frame::SecurityLevel::kSecurityNone)
        {
            frameInfo.mSecurityLevel = frames[i].mSecurityLevel;
            frameInfo.mKeyIdMode     = frames[i].mKeyIdMode;
        }

        frameInfo.PrepareHeadersIn(frame);
        frame.SetAckRequest(false);
        frame.SetIsHeaderUpdated(frames[i].mIsHeaderUpdated);

        {
            Mac::CslIe *cslIe = frame.GetCslIe();

            if (cslIe != nullptr)
            {
                frame.SetCslIe(1000, 200);
            }
        }

        if (frames[i].mPsduLength > 0)
        {
            uint8_t *payload       = frame.GetPayload();
            uint8_t  payloadLength = frames[i].mPsduLength;

            for (uint8_t k = 0; k < payloadLength; k++)
            {
                payload[k] = k;
            }

            frame.SetPayloadLength(payloadLength);
        }

        if ((frames[i].mSecurityLevel != Mac::Frame::SecurityLevel::kSecurityNone) && (!frame.IsHeaderUpdated()))
        {
            frame.SetAesKey(frames[i].mSecurityConfig->mKey);
            frame.SetFrameCounter(frames[i].mSecurityConfig->mFrameCounter);
            frame.SetKeyId((frames[i].mSecurityConfig->mKeySequence & 0x7f) + 1);
            frame.ProcessTransmitAesCcm(*frames[i].mSecurityConfig->mExtAddress);
        }

        OutputFrame(frame, frames[i]);
    }
}

} // namespace ot

int main(void)
{
    printf("Wakeup frames ------------------------------------\r\n");
    ot::TestGenerateWakeupFrames();
    printf("\r\n2015 data frames ---------------------------------\r\n");
    ot::TestGenerate2015Frames();
    return 0;
}

