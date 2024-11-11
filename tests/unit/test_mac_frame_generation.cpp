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

#define OUTPUT_SCRIPT_TEST_FORMAT 1

namespace ot {

// const uint8_t kDstAddress[sizeof(Mac::ExtAddress)] = {0x01, 0xfe, 0xca, 0xef, 0xbe, 0x00, 0xad, 0xde};
// const uint8_t kSrcAddress[sizeof(Mac::ExtAddress)] = {0x02, 0xfe, 0xca, 0xef, 0xbe, 0x00, 0xad, 0xde};

const uint8_t kDstAddress[sizeof(Mac::ExtAddress)] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
const uint8_t kSrcAddress[sizeof(Mac::ExtAddress)] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static const Mac::ShortAddress sDstShortAddress = 0xaaaa;
Mac::ExtAddress                sDstExtAddress;
static const Mac::ShortAddress sSrcShortAddress = 0xbbbb;
Mac::ExtAddress                sSrcExtAddress;
static const Mac::PanId        sDstPanId      = 0xdddd;
static const Mac::PanId        sSrcPanId      = 0xeeee;
static const Mac::PanId        sDstPanIdBcast = Mac::kShortAddrBroadcast;

static Mac::Address sAddressDstExt;
static Mac::Address sAddressSrcExt;
static Mac::Address sAddressDstShortBcast;
static Mac::Address sAddressDstShort;
static Mac::Address sAddressSrcShort;
static Mac::Address sAddressDstNone;
static Mac::Address sAddressSrcNone;

static constexpr Mac::Frame::Version kV2003 = Mac::Frame::kVersion2003;
static constexpr Mac::Frame::Version kV2006 = Mac::Frame::kVersion2006;
static constexpr Mac::Frame::Version kV2015 = Mac::Frame::kVersion2015;

static constexpr Mac::Frame::SecurityLevel kSecNone     = Mac::Frame::SecurityLevel::kSecurityNone;
static constexpr Mac::Frame::SecurityLevel kSecEncMic32 = Mac::Frame::SecurityLevel::kSecurityEncMic32;

static constexpr Mac::Frame::KeyIdMode kKeyIdMode0 = Mac::Frame::KeyIdMode::kKeyIdMode0;
static constexpr Mac::Frame::KeyIdMode kKeyIdMode1 = Mac::Frame::KeyIdMode::kKeyIdMode1;
static constexpr Mac::Frame::KeyIdMode kKeyIdMode2 = Mac::Frame::KeyIdMode::kKeyIdMode2;

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

class FrameConfig
{
public:
    FrameConfig(Mac::Frame::Version        aVersion,
                Mac::Address              *aDestAddress,
                Mac::Address              *aSrcAddress,
                bool                       aIsDstPanIdPresent,
                bool                       aIsSrcPanIdPresent,
                bool                       aIsCslIePresent,
                bool                       aIsSequenceSuppressed,
                bool                       aIsHeaderUpdated,
                Mac::Frame::SecurityLevel  aSecurityLevel,
                Mac::Frame::KeyIdMode      aKeyIdMode,
                bool                       aIsPanIdSame,
                ot2015FrameSecurityConfig *aSecurityConfig,
                uint8_t                    aPsduLength)
        : mType(Mac::Frame::kTypeData)
        , mCommandId(Mac::Frame::kMacCmdDataRequest)
        , mVersion(aVersion)
        , mDestAddress(aDestAddress)
        , mSrcAddress(aSrcAddress)
        , mIsDstPanIdPresent(aIsDstPanIdPresent)
        , mIsSrcPanIdPresent(aIsSrcPanIdPresent)
        , mIsCslIePresent(aIsCslIePresent)
        , mIsSequenceSuppressed(aIsSequenceSuppressed)
        , mIsHeaderUpdated(aIsHeaderUpdated)
        , mSecurityLevel(aSecurityLevel)
        , mKeyIdMode(aKeyIdMode)
        , mIsPanIdSame(aIsPanIdSame)
        , mSecurityConfig(aSecurityConfig)
        , mPsduLength(aPsduLength)
    {
    }
    FrameConfig(Mac::Frame::Version aVersion,
                Mac::Address       *aDestAddress,
                Mac::Address       *aSrcAddress,
                bool                aIsPanIdSame,
                bool                aIsCslIePresent)
        : FrameConfig(aVersion,
                      aDestAddress,
                      aSrcAddress,
                      true /*aIsDstPanIdPresent*/,
                      true /*aIsSrcPanIdPresent*/,
                      aIsCslIePresent,
                      false /*aIsSequenceSuppressed*/,
                      true /*aIsHeaderUpdated*/,
                      kSecNone /*aSecurityLevel*/,
                      kKeyIdMode0 /* kkeyIdMode*/,
                      aIsPanIdSame /*aIsPanIdSame*/,
                      nullptr /*aSecurityConfig*/,
                      0 /*aPsduLength*/)
    {
    }

    FrameConfig(Mac::Frame::Version aVersion,
                Mac::Address       *aDestAddress,
                Mac::Address       *aSrcAddress,
                bool                aIsDstPanIdPresent,
                bool                aIsSrcPanIdPresent,
                bool                aIsCslIePresent)
        : FrameConfig(aVersion,
                      aDestAddress,
                      aSrcAddress,
                      aIsDstPanIdPresent,
                      aIsSrcPanIdPresent,
                      aIsCslIePresent,
                      false /*aIsSequenceSuppressed*/,
                      true /*aIsHeaderUpdated*/,
                      kSecNone /*aSecurityLevel*/,
                      kKeyIdMode0 /* kkeyIdMode*/,
                      false /*aIsPanIdSame*/,
                      nullptr /*aSecurityConfig*/,
                      0 /*aPsduLength*/)
    {
    }

    Mac::Frame::Type      mType;
    Mac::Frame::CommandId mCommandId;
    Mac::Frame::Version   mVersion;
    Mac::Address         *mDestAddress;
    Mac::Address         *mSrcAddress;

    bool mIsDstPanIdPresent;
    bool mIsSrcPanIdPresent;
    bool mIsCslIePresent;
    bool mIsSequenceSuppressed;
    bool mIsHeaderUpdated;

    Mac::Frame::SecurityLevel mSecurityLevel;
    Mac::Frame::KeyIdMode     mKeyIdMode;
    bool                      mIsPanIdSame;

    ot2015FrameSecurityConfig *mSecurityConfig;

    uint8_t mPsduLength;
};

void SecurityConfigToString(ot2015FrameSecurityConfig &aConfig, char *aBuf, uint16_t aLength)
{
    char         buf[64] = {0};
    StringWriter writer(buf, sizeof(buf));

    writer.AppendHexBytes(aConfig.mKey.mKeyMaterial.mKey.m8, sizeof(aConfig.mKey.mKeyMaterial.mKey.m8));
    snprintf(aBuf, aLength, "keySeq=%u,FrameCounter=%u,ExtAddress:%s,Key:%s", aConfig.mKeySequence,
             aConfig.mFrameCounter, aConfig.mExtAddress->ToString().AsCString(), buf);
}

InfoString FrameToInfoString(Mac::TxFrame &aFrame, FrameConfig &aConfig)
{
    InfoString   string;
    uint8_t      commandId, type;
    Mac::Address src, dst;
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
#if OUTPUT_SCRIPT_TEST_FORMAT
            string.Append("sec:l%u", level);
#else
            string.Append("SecLevel:%u,keyIdMode:%u,", level, keyIdMode);
            if (aConfig.mSecurityConfig != nullptr)
            {
                SecurityConfigToString(*aConfig.mSecurityConfig, buf, sizeof(buf));
                string.Append("%s", buf);
            }
#endif
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

void OutputFrame(Mac::TxFrame &aFrame, FrameConfig &aConfig)
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

    sAddressDstShortBcast.SetShort(Mac::kShortAddrBroadcast);
    sAddressDstShort.SetShort(sDstShortAddress);
    sAddressSrcShort.SetShort(sSrcShortAddress);

    sAddressDstNone.SetNone();
    sAddressSrcNone.SetNone();

    memcpy(sSecurityConfig.mKey.mKeyMaterial.mKey.m8, kKey, sizeof(kKey));
    sSecurityConfig.mKeySequence  = 0;
    sSecurityConfig.mFrameCounter = 0;
    sSecurityConfig.mExtAddress   = &sSrcExtAddress;
}

void Generate154Frame(Mac::TxFrame &frame, FrameConfig &aConfig)
{
    Mac::TxFrame::Info frameInfo;

    memset(sPsdu, 0, sizeof(sPsdu));
    frameInfo.Clear();

    frameInfo.mType               = aConfig.mType;
    frameInfo.mVersion            = aConfig.mVersion;
    frameInfo.mEmptyPayload       = true;
    frameInfo.mAddrs.mDestination = *aConfig.mDestAddress;
    frameInfo.mAddrs.mSource      = *aConfig.mSrcAddress;

    if (aConfig.mType == Mac::Frame::kTypeMacCmd)
    {
        frameInfo.mCommandId = aConfig.mCommandId;
    }

    frameInfo.mPanIds.Clear();

    if (aConfig.mIsPanIdSame)
    {
        frameInfo.mPanIds.SetBothSourceDestination(sDstPanId);
    }
    else
    {
        if (aConfig.mIsDstPanIdPresent)
        {
            frameInfo.mPanIds.SetDestination(sDstPanId);
        }

        if (aConfig.mIsSrcPanIdPresent)
        {
            frameInfo.mPanIds.SetSource(sSrcPanId);
        }
    }

    frameInfo.mAppendCslIe      = aConfig.mIsCslIePresent;
    frameInfo.mSuppressSequence = aConfig.mIsSequenceSuppressed;

    if (aConfig.mSecurityLevel != Mac::Frame::SecurityLevel::kSecurityNone)
    {
        frameInfo.mSecurityLevel = aConfig.mSecurityLevel;
        frameInfo.mKeyIdMode     = aConfig.mKeyIdMode;
    }

    frameInfo.PrepareHeadersIn(frame);
    frame.SetAckRequest(false);
    frame.SetIsHeaderUpdated(aConfig.mIsHeaderUpdated);

    {
        Mac::CslIe *cslIe = frame.GetCslIe();

        if (cslIe != nullptr)
        {
            frame.SetCslIe(1000, 200);
        }
    }

    if (aConfig.mPsduLength > 0)
    {
        uint8_t *payload       = frame.GetPayload();
        uint8_t  payloadLength = aConfig.mPsduLength;

        for (uint8_t k = 0; k < payloadLength; k++)
        {
            payload[k] = k;
        }

        frame.SetPayloadLength(payloadLength);
    }

    if ((aConfig.mSecurityLevel != Mac::Frame::SecurityLevel::kSecurityNone) && (!frame.IsHeaderUpdated()))
    {
        frame.SetAesKey(aConfig.mSecurityConfig->mKey);
        frame.SetFrameCounter(aConfig.mSecurityConfig->mFrameCounter);
        frame.SetKeyId((aConfig.mSecurityConfig->mKeySequence & 0x7f) + 1);
        frame.ProcessTransmitAesCcm(*aConfig.mSecurityConfig->mExtAddress);
    }
}

void OutputFrameTestFormat(Mac::TxFrame &aFrame, FrameConfig &aConfig)
{
    char         buf[512] = {0};
    StringWriter writer(buf, sizeof(buf));

    writer.AppendHexBytes(aFrame.GetPsdu(), aFrame.GetLength());
    printf("  self.Frame(name='%s',\r\n", FrameToInfoString(aFrame, aConfig).AsCString());
    printf("             tx_frame='%s',\r\n", buf);

    if (aConfig.mDestAddress->IsExtended())
    {
        printf("             dst_address='%s'", aConfig.mDestAddress->GetExtended().ToString().AsCString());
    }
    else if (aConfig.mDestAddress->IsShort())
    {
        printf("             dst_address='0x%04x'", aConfig.mDestAddress->GetShort());
    }
    else
    {
        printf("             dst_address='-'");
    }

#if !OUTPUT_SCRIPT_TEST_FORMAT
    if (!aConfig.mIsHeaderUpdated)
    {
        printf(",\r\n             src_address='%s'", sAddressSrcExt.GetExtended().ToString().AsCString());
    }
#endif

    printf("),\r\n");
}

void TestGenerateBeaconFrames()
{
    Mac::TxFrame frame;

    FrameConfig configs[] = {
        {kV2003, &sAddressDstShortBcast, &sAddressSrcNone, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},
        {kV2003, &sAddressDstNone, &sAddressSrcExt, false /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/},
    };

    Mac::TxFrame::Info frameInfo;

    printf("\r\n\r\nTestGenerateBeaconFrames():\r\n");

    InitFrameInfo(frame);

    // Beacon Request
    {
        memset(sPsdu, 0, sizeof(sPsdu));
        frameInfo.Clear();
        frameInfo.mAddrs.mSource.SetNone();
        frameInfo.mAddrs.mDestination = sAddressDstShortBcast;
        frameInfo.mPanIds.SetDestination(Mac::kShortAddrBroadcast);

        frameInfo.mType      = Mac::Frame::kTypeMacCmd;
        frameInfo.mCommandId = Mac::Frame::kMacCmdBeaconRequest;
        frameInfo.mVersion   = Mac::Frame::kVersion2003;

        frameInfo.PrepareHeadersIn(frame);
        frame.SetAckRequest(false);

        OutputFrameTestFormat(frame, configs[0]);
    }

    // Beacon
    {
        uint8_t             beaconLength;
        Mac::Beacon        *beacon        = nullptr;
        Mac::BeaconPayload *beaconPayload = nullptr;

        memset(sPsdu, 0, sizeof(sPsdu));
        frameInfo.Clear();

        frameInfo.mAddrs.mSource = sAddressSrcExt;
        frameInfo.mPanIds.SetSource(sSrcPanId);
        frameInfo.mAddrs.mDestination.SetNone();

        frameInfo.mType    = Mac::Frame::kTypeBeacon;
        frameInfo.mVersion = Mac::Frame::kVersion2003;

        frameInfo.PrepareHeadersIn(frame);

        beacon = reinterpret_cast<Mac::Beacon *>(frame.GetPayload());
        beacon->Init();

        beaconLength  = sizeof(*beacon);
        beaconPayload = reinterpret_cast<Mac::BeaconPayload *>(beacon->GetPayload());

        beaconPayload->Init();
        beaconPayload->SetJoiningPermitted();

        {
            static const otExtendedPanId kExtPanId = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
            beaconPayload->SetNetworkName(MeshCoP::NetworkName::kNetworkNameInit);
            beaconPayload->SetExtendedPanId(kExtPanId);
        }

        beaconLength += sizeof(*beaconPayload);

        frame.SetPayloadLength(beaconLength);
        frame.SetAckRequest(false);

        OutputFrameTestFormat(frame, configs[1]);
    }
}

void TestGenerateWakeupFrames()
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;
    Mac::ConnectionIe *connectionIe;
    FrameConfig        config = {kV2003,
                                 &sAddressDstExt,
                                 &sAddressSrcExt,
                                 true /*DstPanId*/,
                                 true /*SrcPanId*/,
                                 false /*CslIe*/,
                                 false /*SeqSuppressed*/,
                                 false /*isHeaderUpdated*/,
                                 kSecEncMic32,
                                 kKeyIdMode2,
                                 true /*aIsPanIdSame*/,
                                 &sSecurityConfig,
                                 0};

    printf("\r\n\r\nTestGenerateWakeupFrames():\r\n");
    InitFrameInfo(frame);

    frameInfo.Clear();

    frame.GenerateWakeupFrame(sDstPanId, sAddressDstExt, sAddressSrcExt);

    frame.GetRendezvousTimeIe()->SetRendezvousTime(1000);
    connectionIe = frame.GetConnectionIe();
    connectionIe->SetRetryInterval(1);
    connectionIe->SetRetryCount(12);

    frame.SetAesKey(sSecurityConfig.mKey);
    frame.SetFrameCounter(sSecurityConfig.mFrameCounter);
    frame.SetKeyId((sSecurityConfig.mKeySequence & 0x7f) + 1);
    frame.ProcessTransmitAesCcm(*sSecurityConfig.mExtAddress);

    OutputFrameTestFormat(frame, config);
}

void TestGenerateDataPollFrames()
{
    Mac::TxFrame frame;

    FrameConfig configs[] = {
        {kV2006, &sAddressDstShort, &sAddressSrcShort, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/,
         false /*SeqSuppressed*/, false /*isHeaderUpdated*/, kSecEncMic32, kKeyIdMode1, true /*aIsPanIdSame*/,
         &sSecurityConfig, 0},
        {kV2006, &sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/,
         false /*SeqSuppressed*/, false /*isHeaderUpdated*/, kSecEncMic32, kKeyIdMode1, true /*aIsPanIdSame*/,
         &sSecurityConfig, 0},
    };

    printf("\r\n\r\nTestGenerateDataPollFrames():\r\n");
    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(configs); i++)
    {
        configs[i].mType      = Mac::Frame::kTypeMacCmd;
        configs[i].mCommandId = Mac::Frame::kMacCmdDataRequest;

        Generate154Frame(frame, configs[i]);
        OutputFrameTestFormat(frame, configs[i]);
    }
}

void TestAllGenerate2006Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;
#if TTEST
    uint8_t frames[][4]{
        // DstAddress, SrcAddress, DstPanId,    SrcPanId
        {kExtended, kExtended, kPresentSame, kPresentSame},
        {kShort, kShort, kPresent, kPresent},
        {kExtended, kNotPresent, kPresentSame, kPresentSame},
        {kShort, kNotPresent, kPresent, kPresent},
    };
#endif

    FrameConfig configs[] = {
        {kV2006, &sAddressDstExt, &sAddressSrcExt, true /* aIsPanIdSame*/, false /*CslIe*/},
        {kV2006, &sAddressDstShort, &sAddressSrcShort, false /* aIsPanIdSame*/, false /*CslIe*/},
        {kV2006, &sAddressDstExt, &sAddressSrcNone, true /* aIsPanIdSame*/, false /*CslIe*/},
        {kV2006, &sAddressDstShort, &sAddressSrcNone, false /* aIsPanIdSame*/, false /*CslIe*/},
    };

    printf("\r\n\r\nTestGenerate2006Frames():\r\n");
    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(configs); i++)
    {
        Generate154Frame(frame, configs[i]);
        OutputFrameTestFormat(frame, configs[i]);
    }
}

void TestAllGenerate2015Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;
#if TTEST
    uint8_t frames[][6]{

        // TX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:extd,pan:id],sec:no,ie:no,plen:0 ----------------- NotSupported

        // DstAddress, SrcAddress, DstPanId,    SrcPanId,    CSLIe,       SuppressSequence
        {kNotPresent, kNotPresent, kNotPresent, kNotPresent, kNotPresent, false}, ///< No 1
        {kNotPresent, kNotPresent, kPresent, kNotPresent, kNotPresent, false},    ///< No 2
        {kExtended, kNotPresent, kPresent, kNotPresent, kNotPresent, false},      ///< No 3
        {kExtended, kNotPresent, kNotPresent, kNotPresent, kNotPresent, false},   ///< No 4
        {kNotPresent, kExtended, kNotPresent, kPresent, kNotPresent, false},      ///< No 5
        {kNotPresent, kExtended, kNotPresent, kNotPresent, kNotPresent, false},   ///< No 6

        {kExtended, kExtended, kPresent, kNotPresent, kNotPresent, false},    ///< No 7
        {kExtended, kExtended, kNotPresent, kNotPresent, kNotPresent, false}, ///< No 8

        {kShort, kShort, kPresent, kPresent, kNotPresent, false},    ///< No 9
        {kShort, kExtended, kPresent, kPresent, kNotPresent, false}, ///< No 10
        {kExtended, kShort, kPresent, kPresent, kNotPresent, false}, ///< No 11
        //{kShort, kExtended, kPresent, kNotPresent, kNotPresent}, ///< No 12
        //{kExtended, kShort, kPresent, kNotPresent, kNotPresent}, ///< No 13
        //{kShort, kShort, kPresent, kNotPresent, kNotPresent},    ///< No 14
        {kShort, kShort, kPresent, kPresent, kPresent, false},
        {kShort, kShort, kPresent, kPresent, kNotPresent, true},
    };
#endif

    FrameConfig configs[] = {
        // Version, DstAddress,    SrcAddress,       DstPanId,           SrcPanId,           CSLIe
        {kV2015, &sAddressDstNone, &sAddressSrcNone, false /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/}, ///< No 1
        {kV2015, &sAddressDstNone, &sAddressSrcNone, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},  ///< No 2
        {kV2015, &sAddressDstExt, &sAddressSrcNone, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},   ///< No 3
        {kV2015, &sAddressDstExt, &sAddressSrcNone, false /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},  ///< No 4
        {kV2015, &sAddressDstNone, &sAddressSrcExt, false /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/},   ///< No 5
        {kV2015, &sAddressDstNone, &sAddressSrcExt, false /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},  ///< No 6

        {kV2015, &sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},    ///< No 7
        {kV2015, &sAddressDstExt, &sAddressSrcExt, false /*DstPanId*/, false /*SrcPanId*/, false /*CslIe*/},   ///< No 8

        {kV2015, &sAddressDstShort, &sAddressSrcShort, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/}, ///< No 9
        {kV2015, &sAddressDstShort, &sAddressSrcExt, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/},   ///< No 10
        {kV2015, &sAddressDstExt, &sAddressSrcShort, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/},   ///< No 11

        {kV2015, &sAddressDstShort, &sAddressSrcShort, true /*DstPanId*/, true /*SrcPanId*/, true /*CslIe*/},  ///< No 9 + CSL IE
        {kV2015, &sAddressDstShort, &sAddressSrcShort, true /*DstPanId*/, true /*SrcPanId*/, false /*CslIe*/}, ///< No 9 + NoSequence
    };

    printf("\r\n\r\nTestGenerate2015Frames():\r\n");
    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(configs); i++)
    {
        if (i == OT_ARRAY_LENGTH(configs) - 1) {
            configs[i].mIsSequenceSuppressed = true;
        }

        Generate154Frame(frame, configs[i]);
        OutputFrameTestFormat(frame, configs[i]);
    }
}

void TestGenerateSpecified2015Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    FrameConfig configs[] = {{kV2015, &sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/,
                              false /*CslIe*/, false /*SeqSuppressed*/, false /*isHeaderUpdated*/, kSecNone,
                              kKeyIdMode0, false /*aIsPanIdSame*/, nullptr, 10},
                             {kV2015, &sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/,
                              false /*CslIe*/, false /*SeqSuppressed*/, true /*isHeaderUpdated*/, kSecEncMic32,
                              kKeyIdMode1, false /*aIsPanIdSame*/, &sSecurityConfig, 10},
                             {kV2015, &sAddressDstExt, &sAddressSrcExt, true /*DstPanId*/, false /*SrcPanId*/,
                              false /*CslIe*/, false /*SeqSuppressed*/, false /*isHeaderUpdated*/, kSecEncMic32,
                              kKeyIdMode1, false /*aIsPanIdSame*/, &sSecurityConfig, 10}};

    InitFrameInfo(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(configs); i++)
    {
        Generate154Frame(frame, configs[i]);
        OutputFrame(frame, configs[i]);
    }
}

} // namespace ot

int main(void)
{
    ot::TestGenerateBeaconFrames();
    ot::TestGenerateWakeupFrames();
    ot::TestGenerateDataPollFrames();
    ot::TestAllGenerate2006Frames();
    ot::TestAllGenerate2015Frames();
    ot::TestGenerateSpecified2015Frames();
    return 0;
}

