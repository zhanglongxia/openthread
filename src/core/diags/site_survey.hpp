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

/**
 * @file
 *   This file contains definitions for the diagnostics module.
 */

#ifndef SITE_SURVEY_HPP_
#define SITE_SURVEY_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_DIAG_ENABLE && OPENTHREAD_CONFIG_DIAG_SITE_SURVEY_ENABLE

#include <string.h>

#include <openthread/diag.h>
#include <openthread/platform/radio.h>

#include "common/clearable.hpp"
#include "common/encoding.hpp"
#include "common/error.hpp"
#include "common/locator.hpp"
#include "common/new.hpp"
#include "common/non_copyable.hpp"
#include "common/string.hpp"
#include "mac/mac_frame.hpp"
#include "mac/mac_types.hpp"

namespace ot {
namespace FactoryDiags {
/*
client                                    Server

SendingRequest      Request(SEQ=0)                         ListeningRequest
                  ---------------------------------------->
                      ACK(SEQ=0)
                  <----------------------------------------
ConEstablished                      ACK(SEQ=0)            RequestReceived
                  ---------------------------------------->
                                                         ConEstablished

SendingData
                      Data                                 ReceivingData
                  ========================================>

WaitingReport                                              SendingReport
                      Report(SEQ=0)
                  <----------------------------------------
                      ACK(SEQ=0)
                  ---------------------------------------->
Disabled                                                   ListeningRequest


             Client                                      Server
               |                                           |
SendingRequest |              Request(SEQ=0)               |  ListeningRequest
               | ----------------------------------------> |
               |          AcceptAndRequest(SEQ=0)          |
               | <---------------------------------------- |
ConEstablished |               Accept(SEQ=0)               |  RequestReceived
               | ----------------------------------------> |
               |                                           |  ConEstablished
               |                                           |
SendingData    |                                           |
               |               15.4 Frame                  |  ReceivingData
               | <=======================================> |
               |                                           |
WaitingReport  |                                           |   SendingReport
               |               Report(SEQ=0)               |
               | <---------------------------------------- |
               |                ACK(SEQ=0)                 |
               | ----------------------------------------> |
Disabled       |                                           |   ListeningRequest
               |                                           |


*/

class SiteSurvey : private NonCopyable
{
public:
    SiteSurvey(Instance &aInstance, otRadioFrame *aFrame);

    Error ProcessCommand(uint8_t aArgsLength, char *aArgs[]);

    void SetOutputCallback(otDiagOutputCallback aCallback, void *aContext);

    bool IsRunning(void) { return (mState != kStateDisabled); }

    Error TimerFired(void);

    void ReceiveDone(const Mac::RxFrame &aFrame, Error aError);

    void TransmitDone(Error aError);

    void SetChannel(uint8_t aChannel) { mChannel = aChannel; }

private:
    static constexpr uint8_t kMaxCsmaBackoffsDirect = OPENTHREAD_CONFIG_MAC_MAX_CSMA_BACKOFFS_DIRECT;
    static constexpr uint8_t kMaxRadioFrameSize     = OT_RADIO_FRAME_MAX_SIZE;

    enum FrameType : uint8_t
    {
        kFrameTypeRequest = 0,
        kFrameTypeAck     = 1,
        kFrameTypeReport  = 2,
        kFrameTypeData    = 3,
        kFrameTypeMask    = 0x03,
    };

    enum State : uint8_t
    {
        kStateDisabled,
        kStateClientSendingRequest,
        kStateServerWaitingRequest,
        kStateServerWaitingAck,
        kStateConnectionEstablished,
        kStateSendingData,
        kStateReceivingData,
        kStateServerSendingReport,
        kStateClientWaitingReport,
    };

    enum Role : uint8_t
    {
        kRoleDisabled = 0,
        kRoleClient   = 1,
        kRoleServer   = 2,
    };

    enum
    {
        kRetryInterval    = 40,
        kReportInterval   = 40,
        kRxGuardTime      = 40,
        kMinDataFrameSize = 14, ///< FCF(2) + Seq(1) + DstExtendedAddr(8) + FrameType(1) + FCS(2)
    };

    OT_TOOL_PACKED_BEGIN
    class Config
    {
    public:
        enum Direction : uint8_t
        {
            kDirectionTx, ///< Sends data frame from client to server.
            kDirectionRx, ///< Sends data frame from server to client.
        };

        Config()
            : mType(kFrameTypeRequest)
            , mDirection(kDirectionTx)
            , mChannel(kDefaultChannel)
            , mMaxAttempts(kDefaultMaxAttempts)
            , mFrameLength(kDefaultFrameLength)
        {
            SetNumFrames(kDefaultNumFrames);
            SetTxInterval(kDefaultTxTnterval);
        }

        void Reset(void);
        void SetChannel(uint8_t aChannel) { mChannel = aChannel; }
        void SetDirection(Direction aDirection) { mDirection = aDirection; }
        void SetMaxAttempts(uint8_t aMaxAttempts) { mMaxAttempts = aMaxAttempts; }
        void SetFrameLength(uint8_t aFrameLength) { mFrameLength = aFrameLength; }
        void SetNumFrames(uint16_t aNumFrames)
        {
            LittleEndian::WriteUint16(aNumFrames, reinterpret_cast<uint8_t *>(&mNumFrames));
        }

        void SetTxInterval(uint16_t aInterval)
        {
            LittleEndian::WriteUint16(aInterval, reinterpret_cast<uint8_t *>(&mTxInterval));
        }

        uint8_t   GetFrameType(void) { return mType; }
        uint8_t   GetChannel(void) { return mChannel; }
        Direction GetDirection(void) { return static_cast<Direction>(mDirection); }
        uint8_t   GetMaxAttempts(void) { return mMaxAttempts; }
        uint8_t   GetFrameLength(void) { return mFrameLength; }
        uint16_t  GetNumFrames(void) { return LittleEndian::ReadUint16(reinterpret_cast<uint8_t *>(&mNumFrames)); }
        uint16_t  GetTxInterval(void) { return LittleEndian::ReadUint16(reinterpret_cast<uint8_t *>(&mTxInterval)); }

    private:
        enum
        {
            kDefaultMaxAttempts = 24,
            kDefaultFrameLength = 64,
            kDefaultNumFrames   = 100,
            kDefaultTxTnterval  = 20,
            kDefaultChannel     = 19,
        };

        uint8_t  mType : 2;
        uint8_t  mDirection : 1;
        uint8_t  mChannel;
        uint8_t  mMaxAttempts;
        uint8_t  mFrameLength;
        uint16_t mNumFrames;
        uint16_t mTxInterval;
    } OT_TOOL_PACKED_END;

    OT_TOOL_PACKED_BEGIN
    class Report
    {
        friend class SiteSurvey;

    public:
        Report()
            : mType(kFrameTypeReport)
            , mNumReceivedFrames(0)
            , mMinRssi(kMaxRssi)
            , mAvgRssi(kMinRssi)
            , mMaxRssi(kMinRssi)
            , mMinLqi(kMaxLqi)
            , mAvgLqi(kMinLqi)
            , mMaxLqi(kMinLqi)
            , mSumRssi(0)
            , mSumLqi(0)
        {
        }

        void Reset(void);
        void SetNumReceivedFrames(uint16_t aNumReceivedFrames)
        {
            LittleEndian::WriteUint16(aNumReceivedFrames, reinterpret_cast<uint8_t *>(&mNumReceivedFrames));
        }

        uint16_t GetNumReceivedFrames(void)
        {
            return LittleEndian::ReadUint16(reinterpret_cast<uint8_t *>(&mNumReceivedFrames));
        }

        void UpdateRssi(int8_t aRssi)
        {
            mMaxRssi = (aRssi > mMaxRssi) ? aRssi : mMaxRssi;
            mMinRssi = (aRssi < mMinRssi) ? aRssi : mMinRssi;
            mSumRssi += aRssi;
        }

        void UpdateLqi(uint8_t aLqi)
        {
            mMaxLqi = (aLqi > mMaxLqi) ? aLqi : mMaxLqi;
            mMinLqi = (aLqi < mMinLqi) ? aLqi : mMinLqi;
            mSumLqi += aLqi;
        }

        void UpdateAvgRssiLqi(void)
        {
            uint16_t num = GetNumReceivedFrames();

            if (num != 0)
            {
                mAvgRssi = mSumRssi / num;
                mAvgLqi  = mSumLqi / num;
            }
        }

        static uint8_t GetSize(void) { return sizeof(Report) - sizeof(mSumRssi) - sizeof(mSumLqi); }

    private:
        enum
        {
            kMinRssi = -127,
            kMaxRssi = 127,
            kMinLqi  = 0,
            kMaxLqi  = 255,
        };

        uint8_t  mType : 2;
        uint16_t mNumReceivedFrames;
        int8_t   mMinRssi;
        int8_t   mAvgRssi;
        int8_t   mMaxRssi;
        uint8_t  mMinLqi;
        uint8_t  mAvgLqi;
        uint8_t  mMaxLqi;

        int64_t  mSumRssi;
        uint64_t mSumLqi;
    } OT_TOOL_PACKED_END;

    bool IsValidFrame(const Mac::RxFrame &aFrame, FrameType aType);
    void BuildFrame(uint8_t        aChannel,
                    uint8_t        aSequence,
                    bool           aIsSrcAddrPresent,
                    const uint8_t *aPayload,
                    uint8_t        aLength);

    void    SendRequestFrame(void);
    void    SendAckFrame(uint8_t aSequence);
    void    SendReportFrame(void);
    void    SendDataFrame(void);
    otError TransmitFrame(Mac::TxFrame &aFrame);
    otError SetRxChannel(uint8_t aChannel);
    void    SetState(State aState);
    void    OutputReport(void);
    void    SetRole(Role aRole) { mRole = aRole; }
    bool    IsClient(void) const { return mRole == kRoleClient; }
    bool    IsServer(void) const { return mRole == kRoleServer; }
    bool    IsDisabled(void) const { return mRole == kRoleDisabled; }

    bool               IsReceiver(void);
    static const char *FrameTypeToString(FrameType aFrameType);
    static const char *StateToString(State aState);
    void               TimerStart(uint32_t aDelay);
    void               TimerStop(void);
    void               LogFrame(const Mac::Frame *aFrame, bool aIsTxFrame);
    void               Output(const char *aFormat, ...);
    void               OutputLine(const char *aFormat, ...);

    Instance       &mInstance;
    Mac::TxFrame   &mTxFrame;
    Mac::ExtAddress mPeerAddress;
    Config          mConfig;
    Report          mReport;
    State           mState;
    Role            mRole;
    uint8_t         mChannel;
    uint16_t        mTxCounter;
    uint8_t         mTxSequence;
    bool            mIsReportReceived;
    bool            mIsAsyncClient;

    otDiagOutputCallback mDiagOutputCallback;
    void                *mDiagOutputCallbackContext;
    uint32_t             mTxTimeStamp;
    uint32_t             mInitStamp;
};
} // namespace FactoryDiags
} // namespace ot

#endif // #if OPENTHREAD_CONFIG_DIAG_ENABLE && OPENTHREAD_CONFIG_DIAG_SITE_SURVEY_ENABLE

#endif // SITE_SURVEY_HPP_
