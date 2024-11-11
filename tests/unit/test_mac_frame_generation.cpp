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

const uint8_t kDstAddress[sizeof(Mac::ExtAddress)] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
const uint8_t kSrcAddress[sizeof(Mac::ExtAddress)] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static const Mac::ShortAddress dstShortAddress = 0xaaaa;
Mac::ExtAddress                dstExtAddress;
static const Mac::ShortAddress srcShortAddress = 0xbbbb;
Mac::ExtAddress                srcExtAddress;
static const Mac::PanId        dstPanId = 0xdddd;
static const Mac::PanId        srcPanId = 0xeeee;

static uint8_t sPsdu[OT_RADIO_FRAME_MAX_SIZE];

enum : uint8_t
{
    kNotPresent  = 0,
    kPresent     = 1,
    kNone        = 2,
    kShort       = 3,
    kExtended    = 4,
    kPresentSame = 5,
};

static constexpr uint16_t       kInfoStringSize = 128; ///< Max chars for `InfoString` (ToInfoString()).
typedef String<kInfoStringSize> InfoString;

InfoString FrameToInfoString(Mac::TxFrame &aFrame)
{
    InfoString   string;
    uint8_t      commandId, type;
    Mac::Address src, dst;
    Mac::PanId   srcPanId, dstPanId;
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

    IgnoreError(aFrame.GetSrcPanId(srcPanId));
    IgnoreError(aFrame.GetDstPanId(dstPanId));

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
        uint8_t level;

        if (aFrame.GetSecurityEnabled())
        {
            aFrame.GetSecurityLevel(level);
            string.Append("sec:l%u,", level);
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

    /*
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
    */
    string.Append(",plen:%u", aFrame.GetPayloadLength());

    return string;
}

void OutputFrame(Mac::TxFrame &aFrame)
{
    char         buf[512] = {0};
    StringWriter writer(buf, sizeof(buf));

    writer.AppendHexBytes(aFrame.GetPsdu(), aFrame.GetLength());
    // printf("%-100s : %s\r\n", FrameToInfoString(aFrame).AsCString(), buf);
    // printf("{'name':'%-100s','name': '%s'},\r\n", FrameToInfoString(aFrame).AsCString(), buf);
    printf(" {'name': '%s',\r\n  'psdu': '%s'},\r\n", FrameToInfoString(aFrame).AsCString(), buf);
}

void InitFrame(Mac::TxFrame &aFrame)
{
    aFrame.mPsdu      = sPsdu;
    aFrame.mLength    = 0;
    aFrame.mRadioType = 0;

    dstExtAddress.Set(kDstAddress, Mac::ExtAddress::kReverseByteOrder);
    srcExtAddress.Set(kSrcAddress, Mac::ExtAddress::kReverseByteOrder);
}

void TestGenerateBeaconFrames()
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    InitFrame(frame);

    // Beacon Request
    {
        frameInfo.Clear();
        frameInfo.mAddrs.mSource.SetNone();
        frameInfo.mAddrs.mDestination.SetShort(Mac::kShortAddrBroadcast);
        frameInfo.mPanIds.SetDestination(Mac::kShortAddrBroadcast);

        frameInfo.mType      = Mac::Frame::kTypeMacCmd;
        frameInfo.mCommandId = Mac::Frame::kMacCmdBeaconRequest;
        frameInfo.mVersion   = Mac::Frame::kVersion2003;

        frameInfo.PrepareHeadersIn(frame);
        frame.SetAckRequest(false);

        OutputFrame(frame);
    }

    // Beacon
    {
        uint8_t             beaconLength;
        Mac::Beacon        *beacon        = nullptr;
        Mac::BeaconPayload *beaconPayload = nullptr;

        frameInfo.Clear();

        frameInfo.mAddrs.mSource.SetExtended(srcExtAddress);
        frameInfo.mPanIds.SetSource(srcPanId);
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

        OutputFrame(frame);
    }
}

void TestGenerateWakeupFrames()
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;
    Mac::Address       dstAddress;
    Mac::Address       srcAddress;
    Mac::ConnectionIe *connectionIe;

    InitFrame(frame);

    frameInfo.Clear();

    dstAddress.SetExtended(dstExtAddress);
    srcAddress.SetExtended(srcExtAddress);

    frame.GenerateWakeupFrame(dstPanId, dstAddress, srcAddress);

    frame.GetRendezvousTimeIe()->SetRendezvousTime(1000);
    connectionIe = frame.GetConnectionIe();
    connectionIe->SetRetryInterval(1);
    connectionIe->SetRetryCount(12);

    OutputFrame(frame);
}

void TestGenerateDataPollFrames()
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    InitFrame(frame);

    // 1
    frameInfo.Clear();

    frameInfo.mAddrs.mDestination.SetShort(dstShortAddress);
    frameInfo.mAddrs.mSource.SetShort(srcShortAddress);
    frameInfo.mPanIds.SetBothSourceDestination(dstPanId);

    frameInfo.mVersion       = Mac::Frame::kVersion2006;
    frameInfo.mType          = Mac::Frame::kTypeMacCmd;
    frameInfo.mCommandId     = Mac::Frame::kMacCmdDataRequest;
    frameInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;
    frameInfo.mKeyIdMode     = Mac::Frame::kKeyIdMode1;
    frameInfo.mEmptyPayload  = true;

    frameInfo.PrepareHeadersIn(frame);
    frame.SetAckRequest(false);
    OutputFrame(frame);

    // 2
    frameInfo.Clear();

    frameInfo.mAddrs.mDestination.SetExtended(dstExtAddress);
    frameInfo.mAddrs.mSource.SetExtended(srcExtAddress);
    frameInfo.mPanIds.SetBothSourceDestination(dstPanId);

    frameInfo.mVersion       = Mac::Frame::kVersion2006;
    frameInfo.mType          = Mac::Frame::kTypeMacCmd;
    frameInfo.mCommandId     = Mac::Frame::kMacCmdDataRequest;
    frameInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;
    frameInfo.mKeyIdMode     = Mac::Frame::kKeyIdMode1;
    frameInfo.mEmptyPayload  = true;

    frameInfo.PrepareHeadersIn(frame);
    frame.SetAckRequest(false);
    OutputFrame(frame);
}

void TestGenerate2006Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    InitFrame(frame);

    // 1
    frameInfo.Clear();
    uint8_t frames[][4]{
        {kExtended, kExtended, kPresentSame, kPresentSame},
        {kShort, kShort, kPresent, kPresent},
        {kExtended, kNotPresent, kPresentSame, kPresentSame},
        {kShort, kNotPresent, kPresent, kPresent},
    };

    frameInfo.Clear();

    frameInfo.mType         = Mac::Frame::kTypeData;
    frameInfo.mVersion      = Mac::Frame::kVersion2006;
    frameInfo.mEmptyPayload = true;

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(frames); i++)
    {
        switch (frames[i][0])
        {
        case kNotPresent:
            frameInfo.mAddrs.mDestination.SetNone();
            break;

        case kShort:
            frameInfo.mAddrs.mDestination.SetShort(dstShortAddress);
            break;

        case kExtended:
            frameInfo.mAddrs.mDestination.SetExtended(dstExtAddress);
            break;
        }

        switch (frames[i][1])
        {
        case kNotPresent:
            frameInfo.mAddrs.mSource.SetNone();
            break;

        case kShort:
            frameInfo.mAddrs.mSource.SetShort(srcShortAddress);
            break;

        case kExtended:
            frameInfo.mAddrs.mSource.SetExtended(srcExtAddress);
            break;
        }

        frameInfo.mPanIds.Clear();
        if (frames[i][2] == kPresent)
        {
            frameInfo.mPanIds.SetDestination(dstPanId);
        }
        else if (frames[i][2] == kPresentSame)
        {
            frameInfo.mPanIds.SetDestination(dstPanId);
        }

        if (frames[i][3] == kPresent)
        {
            frameInfo.mPanIds.SetSource(srcPanId);
        }
        else if (frames[i][3] == kPresentSame)
        {
            frameInfo.mPanIds.SetDestination(dstPanId);
        }

        frameInfo.PrepareHeadersIn(frame);
        frame.SetAckRequest(false);
        /*
                if (frame.GetLength() < 10)
                {
                    uint8_t *payload       = frame.GetPayload();
                    uint8_t  payloadLength = 10 - frame.GetLength();

                    for (uint8_t k = 0; k < payloadLength; k++)
                    {
                        payload[k] = k;
                    }

                    frame.SetPayloadLength(payloadLength);
                }
        */

        OutputFrame(frame);
    }
}

void TestGenerate2015Frames(void)
{
    Error              error = kErrorNone;
    Mac::TxFrame       frame;
    Mac::TxFrame::Info frameInfo;

    uint8_t frames[][6]{
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

    InitFrame(frame);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(frames); i++)
    {
        frameInfo.Clear();

        frameInfo.mType         = Mac::Frame::kTypeData;
        frameInfo.mVersion      = Mac::Frame::kVersion2015;
        frameInfo.mEmptyPayload = true;

        switch (frames[i][0])
        {
        case kNotPresent:
            frameInfo.mAddrs.mDestination.SetNone();
            break;

        case kShort:
            frameInfo.mAddrs.mDestination.SetShort(dstShortAddress);
            break;

        case kExtended:
            frameInfo.mAddrs.mDestination.SetExtended(dstExtAddress);
            break;
        }

        switch (frames[i][1])
        {
        case kNotPresent:
            frameInfo.mAddrs.mSource.SetNone();
            break;

        case kShort:
            frameInfo.mAddrs.mSource.SetShort(srcShortAddress);
            break;

        case kExtended:
            frameInfo.mAddrs.mSource.SetExtended(srcExtAddress);
            break;
        }

        frameInfo.mPanIds.Clear();
        if (frames[i][2] == kPresent)
        {
            frameInfo.mPanIds.SetDestination(dstPanId);
        }

        if (frames[i][3] == kPresent)
        {
            frameInfo.mPanIds.SetSource(srcPanId);
        }

        if (frames[i][4] == kPresent)
        {
            frameInfo.mAppendCslIe = true;
        }

        if (frames[i][5])
        {
            frameInfo.mSuppressSequence = true;
        }

        frameInfo.PrepareHeadersIn(frame);
        frame.SetAckRequest(false);

        {
            Mac::CslIe *cslIe = frame.GetCslIe();

            if (cslIe != nullptr)
            {
                frame.SetCslIe(1000, 200);
            }
        }
        /*
                if (frames[i][5] > 0)
                {
                    uint8_t *payload       = frame.GetPayload();
                    uint8_t  payloadLength = 127 - frame.GetLength();

                    printf("length=%u, payloadLength = %u\r\n", frame.GetLength(), payloadLength);
                    payloadLength = payloadLength > frames[i][5] ? frames[i][5] : payloadLength;
                    printf("Length=%u, payloadLength = %u\r\n", frames[i][5], payloadLength);

                    for (uint8_t k = 0; k < payloadLength; k++)
                    {
                        payload[k] = k;
                    }

                    printf("payloadLength = %u", payloadLength);

                    frame.SetPayloadLength(payloadLength);
                }
        */

        OutputFrame(frame);
    }
}

} // namespace ot

int main(void)
{
    ot::TestGenerateBeaconFrames();
    ot::TestGenerateWakeupFrames();
    ot::TestGenerateDataPollFrames();
    ot::TestGenerate2006Frames();
    ot::TestGenerate2015Frames();

    return 0;
}
