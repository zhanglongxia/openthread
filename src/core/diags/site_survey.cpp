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
 *   This file implements the diagnostics module.
 */

#include "site_survey.hpp"

#if OPENTHREAD_CONFIG_DIAG_ENABLE && OPENTHREAD_CONFIG_DIAG_SITE_SURVEY_ENABLE

#include <stdio.h>
#include <stdlib.h>

#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/diag.h>

#include "common/as_core_type.hpp"
#include "common/code_utils.hpp"
#include "common/locator.hpp"
#include "instance/instance.hpp"
#include "mac/sub_mac.hpp"
#include "radio/radio.hpp"
#include "utils/parse_cmdline.hpp"

namespace ot {
namespace FactoryDiags {

using namespace ot::Utils::CmdLineParser;

RegisterLogModule("SiteSurvey");

#define TIMER_START(aDelay) \
    {                       \
        TimerStart(aDelay); \
    }

SiteSurvey::SiteSurvey(Instance &aInstance, otRadioFrame *aFrame)
    : mInstance(aInstance)
    , mTxFrame(*static_cast<Mac::TxFrame *>(aFrame))
    , mPeerAddress()
    , mConfig()
    , mReport()
    , mState(kStateDisabled)
    , mRole(kRoleDisabled)
    , mChannel(12)
    , mTxCounter(0)
    , mTxSequence(0)
    , mDiagOutputCallback(nullptr)
    , mDiagOutputCallbackContext(nullptr)
{
    mInitStamp = otPlatAlarmMilliGetNow();
}

Error SiteSurvey::ProcessCommand(uint8_t aArgsLength, char *aArgs[])
{
    Error error = kErrorNone;

    VerifyOrExit(aArgsLength > 0, error = kErrorInvalidArgs);

    if (strcmp(aArgs[0], "server") == 0)
    {
        VerifyOrExit(aArgsLength == 2, error = kErrorInvalidArgs);

        if (strcmp(aArgs[1], "start") == 0)
        {
            VerifyOrExit(IsDisabled(), error = kErrorInvalidState);
            SetRole(kRoleServer);
            mConfig.Reset();
            mReport.Reset();

            SetRxChannel(mChannel);
            SetState(kStateServerWaitingRequest);

            OutputLine("Server listening on channel %u, extended address %s", mChannel,
                       mInstance.Get<Mac::SubMac>().GetExtAddress().ToString().AsCString());
        }
        else if (strcmp(aArgs[1], "stop") == 0)
        {
            VerifyOrExit(IsClient(), error = kErrorInvalidState);
            TimerStop();
            SetRxChannel(mChannel);
            SetState(kStateDisabled);
            SetRole(kRoleDisabled);
        }
        else
        {
            error = kErrorInvalidArgs;
        }
    }
    else if (strcmp(aArgs[0], "client") == 0)
    {
        bool   isAsyncClient = false;
        int8_t argIndex      = 1;

        VerifyOrExit(IsDisabled(), error = kErrorInvalidState);
        VerifyOrExit(argIndex < aArgsLength, error = kErrorInvalidArgs);
        if (strcmp(aArgs[argIndex], "async") == 0)
        {
            isAsyncClient = true;
            argIndex++;
        }

        VerifyOrExit(argIndex < aArgsLength, error = kErrorInvalidArgs);
        SuccessOrExit(error = ParseAsHexString(aArgs[argIndex++], mPeerAddress.m8, sizeof(mPeerAddress.m8)));

        mReport.Reset();
        mConfig.Reset();
        mConfig.SetChannel(mChannel);

        for (; argIndex + 1 < aArgsLength; argIndex += 2)
        {
            if (strcmp(aArgs[argIndex], "-r") == 0)
            {
                mConfig.SetDirection(Config::kDirectionRx);
                argIndex--;
            }
            else if (strcmp(aArgs[argIndex], "-a") == 0)
            {
                uint8_t attempts;

                SuccessOrExit(error = ParseAsUint8(aArgs[argIndex + 1], attempts));
                VerifyOrExit(attempts > 0, error = kErrorInvalidArgs);
                mConfig.SetMaxAttempts(attempts);
            }
            else if (strcmp(aArgs[argIndex], "-c") == 0)
            {
                uint8_t channel;

                SuccessOrExit(error = ParseAsUint8(aArgs[argIndex + 1], channel));
                VerifyOrExit(channel >= Radio::kChannelMin && channel <= Radio::kChannelMax, error = kErrorInvalidArgs);
                mConfig.SetChannel(channel);
            }
            else if (strcmp(aArgs[argIndex], "-l") == 0)
            {
                uint8_t length;

                SuccessOrExit(error = ParseAsUint8(aArgs[argIndex + 1], length));
                VerifyOrExit((length >= kMinDataFrameSize) && (length <= kMaxRadioFrameSize),
                             error = kErrorInvalidArgs);
                mConfig.SetFrameLength(length);
            }
            else if (strcmp(aArgs[argIndex], "-n") == 0)
            {
                uint16_t number;

                SuccessOrExit(error = ParseAsUint16(aArgs[argIndex + 1], number));
                VerifyOrExit(number > 0, error = kErrorInvalidArgs);
                mConfig.SetNumFrames(number);
            }
            else if (strcmp(aArgs[argIndex], "-i") == 0)
            {
                uint16_t interval;

                SuccessOrExit(error = ParseAsUint16(aArgs[argIndex + 1], interval));
                VerifyOrExit(interval > 0, error = kErrorInvalidArgs);
                mConfig.SetTxInterval(interval);
            }
            else
            {
                ExitNow(error = kErrorInvalidArgs);
            }
        }

        mTxSequence       = 0;
        mTxCounter        = 0;
        mIsReportReceived = false;
        mIsAsyncClient    = isAsyncClient;

        SetRole(kRoleClient);
        SetRxChannel(mChannel);

        OutputLine("Client connecting to %s, channel %u", mPeerAddress.ToString().AsCString(), mChannel);
        TIMER_START(kRetryInterval);
        SetState(kStateClientSendingRequest);

        if (!mIsAsyncClient)
        {
            error = kErrorPending;
        }
    }
    else
    {
        error = kErrorInvalidArgs;
    }

exit:
    return error;
}

bool SiteSurvey::IsValidFrame(const Mac::RxFrame &aFrame, FrameType aType)
{
    bool    ret = false;
    uint8_t length;

    VerifyOrExit((aFrame.GetType() == Mac::Frame::kTypeData) && !aFrame.GetAckRequest() && aFrame.IsDstAddrPresent() &&
                 !aFrame.IsSrcPanIdPresent() && !aFrame.IsDstPanIdPresent());
    VerifyOrExit(aFrame.GetPayloadLength() >= 1);
    VerifyOrExit((aFrame.GetPayload()[0] & kFrameTypeMask) == aType);

    switch (aType)
    {
    case kFrameTypeRequest:
        length = sizeof(Config);
        VerifyOrExit(aFrame.IsSrcAddrPresent());
        break;
    case kFrameTypeAck:
        length = sizeof(kFrameTypeAck);
        break;
    case kFrameTypeReport:
        length = Report::GetSize();
        break;
    case kFrameTypeData:
        length = aFrame.GetPayloadLength();
        break;
    default:
        ExitNow();
        break;
    }

    VerifyOrExit(aFrame.GetPayloadLength() == length);

    ret = true;

exit:
    return ret;
}

void SiteSurvey::BuildFrame(uint8_t        aChannel,
                            uint8_t        aSequence,
                            bool           aIsSrcAddrPresent,
                            const uint8_t *aPayload,
                            uint8_t        aLength)
{
    Mac::TxFrame::Info frameInfo;

    if (aIsSrcAddrPresent)
    {
        frameInfo.mAddrs.mSource.SetExtended(mInstance.Get<Mac::SubMac>().GetExtAddress());
    }

    frameInfo.mAddrs.mDestination.SetExtended(mPeerAddress);

    frameInfo.mType             = Mac::Frame::kTypeData;
    frameInfo.mVersion          = Mac::Frame::kVersion2015;
    frameInfo.mSecurityLevel    = Mac::Frame::kSecurityNone;
    frameInfo.mSuppressSequence = false;
    frameInfo.PrepareHeadersIn(mTxFrame);

    mTxFrame.SetChannel(aChannel);
    mTxFrame.SetSequence(aSequence);
    mTxFrame.SetAckRequest(false /* aAckRequest */);
    mTxFrame.SetCsmaCaEnabled(true /* aCsmaCaEnabled */);
    mTxFrame.SetMaxCsmaBackoffs(kMaxCsmaBackoffsDirect);

    if ((aPayload != nullptr) && (aLength != 0))
    {
        memcpy(mTxFrame.GetPayload(), aPayload, aLength);
        mTxFrame.SetPayloadLength(aLength);
    }
    else
    {
        mTxFrame.SetPayloadLength(0);
    }
}

void SiteSurvey::SendRequestFrame(void)
{
    BuildFrame(mChannel, mTxSequence++, true /* aIsSrcAddrPresent */, reinterpret_cast<const uint8_t *>(&mConfig),
               static_cast<uint8_t>(sizeof(mConfig)));
    IgnoreError(TransmitFrame(mTxFrame));
    mTxCounter++;
}

void SiteSurvey::SendAckFrame(uint8_t aSequence)
{
    uint8_t payload = kFrameTypeAck;

    BuildFrame(mChannel, aSequence, false /* aIsSrcAddrPresent */, &payload, sizeof(payload));
    IgnoreError(TransmitFrame(mTxFrame));
}

void SiteSurvey::SendReportFrame(void)
{
    BuildFrame(mChannel, mTxSequence++, false /* aIsSrcAddrPresent */, reinterpret_cast<const uint8_t *>(&mReport),
               Report::GetSize());
    IgnoreError(TransmitFrame(mTxFrame));
    mTxCounter++;
}

void SiteSurvey::SendDataFrame(void)
{
    uint8_t frameLength = mConfig.GetFrameLength();
    uint8_t payload[kMaxRadioFrameSize];
    uint8_t payloadLength;

    OT_ASSERT(frameLength >= kMinDataFrameSize);

    payloadLength = frameLength - (kMinDataFrameSize - sizeof(kFrameTypeData));
    payload[0]    = kFrameTypeData;

    for (uint8_t i = 1; i < payloadLength; i++)
    {
        payload[i] = i;
    }

    BuildFrame(mConfig.GetChannel(), mTxSequence++, false /* aIsSrcAddrPresent */, payload, payloadLength);
    mTxFrame.SetCsmaCaEnabled(false /* aCsmaCaEnabled */);

    IgnoreError(TransmitFrame(mTxFrame));
    mTxCounter++;
}

otError SiteSurvey::TransmitFrame(Mac::TxFrame &aFrame)
{
    otError error;

    mTxTimeStamp = otPlatAlarmMilliGetNow();
    SuccessOrExit(error = mInstance.Get<Radio>().Transmit(aFrame));
    LogFrame(&aFrame, true /* aIsTxFrame */);

exit:
    return error;
}

otError SiteSurvey::SetRxChannel(uint8_t aChannel)
{
    otPlatDiagChannelSet(aChannel);
    return mInstance.Get<Radio>().Receive(aChannel);
}

void SiteSurvey::SetState(State aState)
{
    LogDebg("State: %s -> %s", StateToString(mState), StateToString(aState));
    mState = aState;
}

void SiteSurvey::OutputReport(void)
{
    uint32_t lossRate;

    lossRate = (mConfig.GetNumFrames() - mReport.mNumReceivedFrames) * 1000 / mConfig.GetNumFrames();

    OutputLine("Report: Direction:%s, Ch:%u, Len:%u, Sent:%u, Received:%u, LossRate:%d.%d%%, MinRssi:%d, AvgRssi:%d, "
               "MaxRssi:%d, MinLqi:%u, "
               "AvgLqi:%u, MaxLqi:%u",
               mConfig.GetDirection() == Config::kDirectionTx ? "tx" : "rx", mConfig.GetChannel(),
               mConfig.GetFrameLength(), mConfig.GetNumFrames(), mReport.mNumReceivedFrames, lossRate / 10,
               lossRate % 10, mReport.mMinRssi, mReport.mAvgRssi, mReport.mMaxRssi, mReport.mMinLqi, mReport.mAvgLqi,
               mReport.mMaxLqi);
}

Error SiteSurvey::TimerFired(void)
{
    uint16_t delay;

    switch (mState)
    {
        /*----------------client-----------------*/
    case kStateClientSendingRequest:
        if (mTxCounter < mConfig.GetMaxAttempts())
        {
            SendRequestFrame();
            TIMER_START(kRetryInterval);
        }
        else
        {
            // No ACK is received from the server.
            SetState(kStateDisabled);
            SetRole(kRoleDisabled);
            OutputLine("Failed to connect with %s", mPeerAddress.ToString().AsCString());
            if (!mIsAsyncClient)
            {
                Output("OT_ERROR_NONE");
            }
        }
        break;

    case kStateClientWaitingReport:
        // Reports have been sent out.
        SetState(kStateDisabled);
        SetRole(kRoleDisabled);

        if (mIsReportReceived)
        {
            OutputReport();
            OutputLine("Disconnected from %s", mPeerAddress.ToString().AsCString());
        }
        else
        {
            OutputLine("Disconnected from %s, timeout", mPeerAddress.ToString().AsCString());
        }

        if (!mIsAsyncClient)
        {
            Output("OT_ERROR_NONE");
        }
        break;

        /*----------------server-----------------*/
    case kStateServerWaitingAck:
        if (mTxCounter < mConfig.GetMaxAttempts())
        {
            SendAckFrame(mTxSequence++);
            mTxCounter++;
            TIMER_START(kRetryInterval);
        }
        else
        {
            // Failed to receive ACK from client, timeout
            SetState(kStateServerWaitingRequest);
            OutputLine("Disconnected from %s, timeout", mPeerAddress.ToString().AsCString());
        }
        break;

    case kStateServerSendingReport:
        if (mTxCounter < mConfig.GetMaxAttempts())
        {
            SendReportFrame();
            TIMER_START(kReportInterval);
        }
        else
        {
            SetState(kStateServerWaitingRequest);
            OutputLine("Disconnected from %s, timeout", mPeerAddress.ToString().AsCString());
        }
        break;

        /*----------------common-----------------*/
    case kStateConnectionEstablished:
        if (IsServer())
        {
            if (mConfig.GetDirection() == Config::kDirectionTx)
            {
                mReport.Reset();

                delay = mConfig.GetNumFrames() * mConfig.GetTxInterval() + kRxGuardTime;
                TIMER_START(delay);
                SetState(kStateReceivingData);
            }
            else
            {
                mTxCounter  = 0;
                mTxSequence = 0;
                SetState(kStateSendingData);
                TIMER_START(kRxGuardTime);
            }
        }
        else
        {
            if (mConfig.GetDirection() == Config::kDirectionTx)
            {
                mTxCounter  = 0;
                mTxSequence = 0;

                // Delay to gurantee that the receiver has switched to receive mode.
                SetState(kStateSendingData);
                TIMER_START(kRxGuardTime);
            }
            else
            {
                delay = mConfig.GetNumFrames() * mConfig.GetTxInterval() + kRxGuardTime;
                SetState(kStateReceivingData);
                TIMER_START(delay);
            }
        }
        SetRxChannel(mConfig.GetChannel());
        break;

    case kStateSendingData:
        if (mTxCounter < mConfig.GetNumFrames())
        {
            SendDataFrame();
            OutputLine("TX, Seq=%u, Ch=%u, Len=%u", mTxFrame.GetSequence(), mConfig.GetChannel(), mTxFrame.GetLength());
            TIMER_START(mConfig.GetTxInterval());
        }
        else
        {
            if (IsServer())
            {
                TimerStop();
                SetState(kStateServerWaitingRequest);
                OutputLine("Disconnected from %s", mPeerAddress.ToString().AsCString());
            }
            else
            {
                uint32_t timeout;

                // Set timeout time that waiting for the report.
                timeout = (mConfig.GetMaxAttempts() + 1) * kReportInterval;
                TIMER_START(timeout);
                SetState(kStateClientWaitingReport);
            }

            SetRxChannel(mChannel);
        }
        break;

    case kStateReceivingData:
        // All frames should have been sent out
        mReport.UpdateAvgRssiLqi();

        if (IsServer())
        {
            mTxSequence = 0;
            mTxCounter  = 0;
            TIMER_START(kStateServerSendingReport);
            SetState(kStateServerSendingReport);
        }
        else
        {
            SetState(kStateDisabled);
            OutputReport();
            OutputLine("Disconnected from %s", mPeerAddress.ToString().AsCString());
        }
        SetRxChannel(mChannel);
        break;

    default:
        break;
    }

    return kErrorNone;
}

void SiteSurvey::ReceiveDone(const Mac::RxFrame &aFrame, Error aError)
{
    uint32_t     delay;
    uint32_t     timeout;
    Mac::Address dstAddr;

    VerifyOrExit(aError == kErrorNone);
    VerifyOrExit((aFrame.GetPayloadLength() >= 1) && aFrame.IsDstAddrPresent());
    SuccessOrExit(aFrame.GetDstAddr(dstAddr));
    VerifyOrExit(dstAddr.IsExtended());

    LogFrame(&aFrame, false /* aIsTxFrame */);

    switch (mState)
    {
    /*-----------------client-----------------------------*/
    case kStateClientSendingRequest:
    {
        uint32_t txTimeStamp = mTxTimeStamp;
        uint32_t delta;

        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeAck));
        VerifyOrExit((aFrame.GetSequence() + 1) == mTxSequence);

        TimerStop();
        SendAckFrame(aFrame.GetSequence());

        delta = otPlatAlarmMilliGetNow() - txTimeStamp;
        delay = (mConfig.GetMaxAttempts() - mTxCounter) * kRetryInterval - delta;
        TIMER_START(delay);
        SetState(kStateConnectionEstablished);

        OutputLine("Connected with %s", mPeerAddress.ToString().AsCString());
    }
    break;

    case kStateClientWaitingReport:
    {
        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeReport));

        if (!mIsReportReceived)
        {
            memcpy(&mReport, aFrame.GetPayload(), sizeof(Report));

            mIsReportReceived = true;
        }

        SendAckFrame(aFrame.GetSequence());
    }
    break;

    /*-----------------server-----------------------------*/
    case kStateServerWaitingRequest:
    {
        Mac::Address srcAddr;

        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeRequest));
        SuccessOrExit(aFrame.GetSrcAddr(srcAddr));
        VerifyOrExit(srcAddr.IsExtended());
        memcpy(&mConfig, aFrame.GetPayload(), sizeof(Config));
        VerifyOrExit(mConfig.GetMaxAttempts() > aFrame.GetSequence());

        mPeerAddress = srcAddr.GetExtended();

        mTxSequence = aFrame.GetSequence();
        SendAckFrame(mTxSequence++);
        mTxCounter = mTxSequence;

        TIMER_START(kRetryInterval);
        SetState(kStateServerWaitingAck);
    }
    break;

    case kStateServerWaitingAck:
        if (IsValidFrame(aFrame, kFrameTypeAck))
        {
            uint32_t txTimeStamp = mTxTimeStamp;
            uint32_t delta;
            VerifyOrExit(aFrame.GetSequence() + 1 == mTxSequence);

            delta   = otPlatAlarmMilliGetNow() - txTimeStamp;
            timeout = (mConfig.GetMaxAttempts() - aFrame.GetSequence()) * kRetryInterval - delta;
            TIMER_START(timeout);
            SetState(kStateConnectionEstablished);

            OutputLine("Connected with %s", mPeerAddress.ToString().AsCString());
        }
        else if (IsValidFrame(aFrame, kFrameTypeRequest))
        {
            mTxSequence = aFrame.GetSequence();

            // The client failed to receive the previous sent ACK.
            SendAckFrame(mTxSequence++);
            mTxCounter = mTxSequence;
        }
        break;

    case kStateServerSendingReport:
        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeAck));
        VerifyOrExit(aFrame.GetSequence() + 1 == mTxSequence);
        TimerStop();
        SetState(kStateServerWaitingRequest);
        OutputReport();
        OutputLine("Disconnected from %s", mPeerAddress.ToString().AsCString());
        break;

    /*-----------------common-----------------------------*/
    case kStateConnectionEstablished:
        VerifyOrExit(IsClient());
        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeAck));
        SendAckFrame(aFrame.GetSequence());
        break;

    case kStateSendingData:
        // Do nothing
        break;

    case kStateReceivingData:
        VerifyOrExit(IsReceiver());
        VerifyOrExit(IsValidFrame(aFrame, kFrameTypeData));

        mReport.UpdateRssi(aFrame.GetRssi());
        mReport.UpdateLqi(aFrame.GetLqi());
        mReport.SetNumReceivedFrames(mReport.GetNumReceivedFrames() + 1);

        OutputLine("RX, Seq=%u, Ch=%u, Len=%u, Rssi=%d, Lqi=%u", aFrame.GetSequence(), mConfig.GetChannel(),
                   aFrame.GetLength(), aFrame.GetRssi(), aFrame.GetLqi());

        timeout = (mConfig.GetNumFrames() - aFrame.GetSequence()) * mConfig.GetTxInterval() + kRxGuardTime;
        TIMER_START(timeout);
        break;

    default:
        break;
    }

exit:
    return;
}

bool SiteSurvey::IsReceiver(void)
{
    return (IsServer() && (mConfig.GetDirection() == Config::kDirectionTx)) ||
           (IsClient() && (mConfig.GetDirection() == Config::kDirectionRx));
}

void SiteSurvey::SetOutputCallback(otDiagOutputCallback aCallback, void *aContext)
{
    mDiagOutputCallback        = aCallback;
    mDiagOutputCallbackContext = aContext;
}

const char *SiteSurvey::FrameTypeToString(FrameType aFrameType)
{
    static const char *const kFrameTypeStrings[] = {
        "Request", // (0) kFrameTypeRequest
        "Ack",     // (1) kFrameTypeAck
        "Report",  // (2) kFrameTypeReport
        "Data"     // (3) kFrameTypeData
    };

    static_assert(kFrameTypeRequest == 0, "kFrameTypeRequest value is incorrect");
    static_assert(kFrameTypeAck == 1, "kFrameTypeAck value is incorrect");
    static_assert(kFrameTypeReport == 2, "kFrameTypeReport value is incorrect");
    static_assert(kFrameTypeData == 3, "kFrameTypeData value is incorrect");

    return kFrameTypeStrings[aFrameType];
}

const char *SiteSurvey::StateToString(State aState)
{
    static const char *const kStateStrings[] = {
        "Disabled",              // (0) kStateDisabled
        "ClientSendingRequest",  // (1) kStateClientSendingRequest
        "ServerWaitingRequest",  // (2) kStateServerWaitingRequest
        "ServerWaitingAck",      // (3) kStateServerWaitingAck
        "ConnectionEstablished", // (4) kStateConnectionEstablished
        "SendingData",           // (5) kStateSendingData
        "ReceivingData",         // (6) kStateReceivingData
        "ServerSendingReport",   // (7) kStateServerSendingReport
        "ClientWaitingReport",   // (8) kStateClientWaitingReport
    };

    static_assert(kStateDisabled == 0, "kStateDisabled value is incorrect");
    static_assert(kStateClientSendingRequest == 1, "kStateClientSendingRequest value is incorrect");
    static_assert(kStateServerWaitingRequest == 2, "kStateServerWaitingRequest value is incorrect");
    static_assert(kStateServerWaitingAck == 3, "kStateServerWaitingAck value is incorrect");
    static_assert(kStateConnectionEstablished == 4, "kStateConnectionEstablished value is incorrect");
    static_assert(kStateSendingData == 5, "kStateSendingData value is incorrect");
    static_assert(kStateReceivingData == 6, "kStateReceivingData value is incorrect");
    static_assert(kStateServerSendingReport == 7, "kStateServerSendingReport value is incorrect");
    static_assert(kStateClientWaitingReport == 8, "kStateClientWaitingReport value is incorrect");

    return kStateStrings[aState];
}

void SiteSurvey::TimerStart(uint32_t aDelay) { otPlatAlarmMilliStartAt(&mInstance, otPlatAlarmMilliGetNow(), aDelay); }

void SiteSurvey::TimerStop(void) { otPlatAlarmMilliStop(&mInstance); }

void SiteSurvey::LogFrame(const Mac::Frame *aFrame, bool aIsTxFrame)
{
#if OT_SHOULD_LOG_AT(OT_LOG_LEVEL_DEBG)
    FrameType type;

    VerifyOrExit((aFrame->GetPayloadLength() >= 1) && aFrame->IsDstAddrPresent());
    type = static_cast<FrameType>(aFrame->GetPayload()[0] & kFrameTypeMask);
    LogDebg("[Seq=%u] %s %s", aFrame->GetSequence(), aIsTxFrame ? "Sent" : "Received", FrameTypeToString(type));

exit:
    return;
#else
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aIsTxFrame);
#endif
}

void SiteSurvey::Output(const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);
    if (mDiagOutputCallback != nullptr)
    {
        mDiagOutputCallback(aFormat, args, mDiagOutputCallbackContext);
    }
    va_end(args);
}

void SiteSurvey::OutputLine(const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);
    if (mDiagOutputCallback != nullptr)
    {
        // uint32_t time = (otPlatAlarmMilliGetNow() - mInitStamp);
        // Output("%4u.%03u: ", time / 1000, time % 1000);
        mDiagOutputCallback(aFormat, args, mDiagOutputCallbackContext);
    }
    va_end(args);

    Output("\r\n");
}

void SiteSurvey::TransmitDone(Error aError) { OT_UNUSED_VARIABLE(aError); }

void SiteSurvey::Report::Reset(void) { new (this) Report(); }
void SiteSurvey::Config::Reset(void) { new (this) Config(); }

} // namespace FactoryDiags
} // namespace ot
#endif // #if OPENTHREAD_CONFIG_DIAG_ENABLE && OPENTHREAD_CONFIG_DIAG_SITE_SURVEY_ENABLE
