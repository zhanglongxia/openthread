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
 *   This file implements the radio driver of the subset of IEEE 802.15.4 MAC primitives.
 */

#include "sub_mac.hpp"

#include <openthread/platform/radio.h>

#include "common/code_utils.hpp"
#include "common/locator_getters.hpp"
#include "common/log.hpp"
#include "instance/instance.hpp"
#include "mac/link_metrics.hpp"

namespace ot {
namespace Mac {

RegisterLogModule("SubMac");

void SubMac::RadioInit(void)
{
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    otLinkMetricsInit(kRadioNoiseFloor);
#endif
}

uint16_t SubMac::GetCslPhase(void)
{
    uint32_t curTime       = otPlatAlarmMicroGetNow();
    uint32_t cslPeriodInUs = mCslPeriod * OT_US_PER_TEN_SYMBOLS;
    uint32_t diff =
        (cslPeriodInUs - (curTime % cslPeriodInUs) + (mCslSampleTime.GetValue() % cslPeriodInUs)) % cslPeriodInUs;

    return (uint16_t)(diff / OT_US_PER_TEN_SYMBOLS + 1);
}

Error SubMac::UpdateDataIe(otRadioFrame *aFrame)
{
    Error    error           = kErrorNone;
    bool     processSecurity = false;
    TxFrame &frame           = *reinterpret_cast<TxFrame *>(aFrame);

    OT_ASSERT(aFrame != nullptr);

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if ((mCslPeriod > 0) && !frame.IsARetransmission())
    {
        frame.SetCslIe(mCslPeriod, GetCslPhase());
    }
#endif

    // Update IE and secure transmit frame
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    if (aFrame.GetTimeIeOffset() != 0)
    {
        TimeIe  &timeIe = *reinterpret_cast<TimeIe *>(frame.GetPsdu() + frame.GetTimeIeOffset());
        uint64_t time   = otPlatTimeGet() + frame.GetNetworkTimeOffset();

        timeIe.SetSequence(frame.GetTimeSyncSeq());
        timeIe.SetTime(time);

        processSecurity = true;
    }
#endif

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    VerifyOrExit(frame.GetSecurityEnabled());

    {
        uint8_t keyIdMode;

        SuccessOrExit(error = frame.GetKeyIdMode(keyIdMode));
        VerifyOrExit(keyIdMode == Frame::kKeyIdMode1);
    }
    VerifyOrExit(!frame.IsSecurityProcessed());

    frame.SetAesKey(mCurrKey);

    if (!frame.IsHeaderUpdated())
    {
        frame.SetKeyId(mKeyId);
        frame.SetFrameCounter(mFrameCounter++);
    }

    processSecurity = true;
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

    VerifyOrExit(processSecurity);

    frame.ProcessTransmitAesCcm(mExtAddress);

exit:
    return error;
}

uint8_t SubMac::UpdateCslIe(uint8_t *aIeData, const Address &aDest)
{
    uint8_t len = 0;

    OT_ASSERT(aIeData != nullptr);

    VerifyOrExit(mCslPeriod > 0);
    VerifyOrExit((aDest.IsShort() && (aDest.GetShort() == mCslPeerShort)) ||
                 (aDest.IsExtended() && (aDest.GetExtended() == mCslPeerExt)));

    {
        HeaderIe &headerIe = *reinterpret_cast<HeaderIe *>(aIeData);
        CslIe    &cslIe    = *reinterpret_cast<CslIe *>(aIeData + sizeof(HeaderIe));

        headerIe.Init(CslIe::kHeaderIeId, CslIe::kIeContentSize);
        cslIe.SetPeriod(mCslPeriod);
        cslIe.SetPhase(GetCslPhase());

        len = sizeof(HeaderIe) + sizeof(CslIe);
    }

exit:
    return len;
}

uint8_t SubMac::UpdateLinkMetricsIe(uint8_t *aIeData, const Address &aDestAddress, int8_t aRssi, int8_t aLqi)
{
    uint8_t len = 0;
    uint8_t linkMetricsIeData[OT_ENH_PROBING_IE_DATA_MAX_SIZE];
    uint8_t linkMetricsIeLength;

    OT_ASSERT(aIeData != nullptr);

    linkMetricsIeLength = otLinkMetricsEnhAckGenData(&aDestAddress, aLqi, aRssi, linkMetricsIeData);
    VerifyOrExit(linkMetricsIeLength > 0);

    {
        HeaderIe       &headerIe       = *reinterpret_cast<HeaderIe *>(aIeData);
        VendorIeHeader &vendorHeaderIe = *reinterpret_cast<VendorIeHeader *>(aIeData + sizeof(HeaderIe));

        headerIe.Init(ThreadIe::kHeaderIeId, sizeof(VendorIeHeader) + linkMetricsIeLength);
        vendorHeaderIe.SetVendorOui(ThreadIe::kVendorOuiThreadCompanyId);
        vendorHeaderIe.SetSubType(ThreadIe::kEnhAckProbingIe);
        len = sizeof(HeaderIe) + sizeof(VendorIeHeader);

        memcpy(aIeData + len, linkMetricsIeData, linkMetricsIeLength);
        len += linkMetricsIeLength;
    }

exit:
    return len;
}

Error SubMac::UpdateAckIe(otRadioFrame *aAckFrame, int8_t aRssi, int8_t aLqi)
{
    OT_UNUSED_VARIABLE(aAckFrame);
    OT_UNUSED_VARIABLE(aRssi);
    OT_UNUSED_VARIABLE(aLqi);

    otError  error = kErrorNone;
    TxFrame *frame = reinterpret_cast<TxFrame *>(aAckFrame);

    OT_ASSERT(aAckFrame != nullptr);

    sAckedWithFramePending = frame->GetFramePending();

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE

    Address  destAddress;
    uint8_t *ieDataStart;
    uint8_t *ieData;

    VerifyOrExit(frame->IsVersion2015(), error = kErrorInvalidArgs);
    SuccessOrExit(error = frame->GetDstAddr(destAddress));

    ieData      = frame->GetHeaderIe();
    ieDataStart = ieData;
    VerifyOrExit(ieData != nullptr);

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    ieData += UpdateCslIe(ieData, destAddress);
#endif

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    ieData += UpdateLinkMetricsIe(ieData, destAddress, aRssi, aLqi);
#endif

    frame->SetIePresent(ieData != ieDataStart);

    TxAckProcessSecurity(*frame);

exit:
#endif // OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    return error;
}

void SubMac::TxAckProcessSecurity(TxFrame &aAckFrame)
{
    KeyMaterial *key = nullptr;
    uint8_t      keyId;

    sAckedWithSecEnhAck = false;
    VerifyOrExit(aAckFrame.GetSecurityEnabled());
    VerifyOrExit((aAckFrame.GetKeyId(keyId) == kErrorNone) && (keyId != 0));

    if (keyId == mKeyId)
    {
        key              = &mCurrKey;
        sAckFrameCounter = mFrameCounter++;
    }
    else if (keyId == mKeyId - 1)
    {
        key              = &mPrevKey;
        sAckFrameCounter = mPrevFrameCounter++;
    }
    else if (keyId == mKeyId + 1)
    {
        key = &mNextKey;
        // Openthread does not maintain future frame counter.
        // Mac frame counter would be overwritten after key rotation leading to
        // frames being dropped due to counter value lower than in acks.
        sAckFrameCounter = 0;
    }
    else
    {
        ExitNow();
    }

    sAckKeyId           = keyId;
    sAckedWithSecEnhAck = true;

    aAckFrame.SetAesKey(*key);
    aAckFrame.SetKeyId(keyId);
    aAckFrame.SetFrameCounter(sAckFrameCounter);
    aAckFrame.ProcessTransmitAesCcm(mExtAddress);

exit:
    return;
}

void SubMac::UpdateRxFrameAckInfo(otRadioFrame *aFrame)
{
    RxFrame *frame = reinterpret_cast<RxFrame *>(aFrame);

    OT_ASSERT(frame != nullptr);

    if (frame->GetAckRequest())
    {
        frame->SetAckedWithFramePending(sAckedWithFramePending);
    }
    else
    {
        frame->SetAckedWithFramePending(false);
    }

    VerifyOrExit(frame->GetAckRequest() && frame->IsVersion2015());

    frame->SetAckedWithSecEnhAck(sAckedWithSecEnhAck);
    frame->SetAckFrameCounter(sAckFrameCounter);
    frame->SetAckKeyId(sAckKeyId);

exit:
    sAckedWithFramePending = false;
    sAckedWithSecEnhAck    = false;
    return;
}

} // namespace Mac
} // namespace ot
