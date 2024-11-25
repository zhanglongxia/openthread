/*
 *  Copyright (c) 2025, The OpenThread Authors.
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
 *   This file implements the radio scheduler.
 */

#include "radio_scheduler.hpp"

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
#include "common/code_utils.hpp"
#include "common/locator.hpp"
#include "common/log.hpp"
#include "instance/instance.hpp"
#include "radio/radio.hpp"

namespace ot {
RegisterLogModule("RadioScheduler");

RadioInterface::RadioInterface(Instance &aInstance, Priority aReceivePriority)
    : InstanceLocator(aInstance)
    , mState(kStateDisabled)
    , mCurPriority(kPriorityMax)
    , mReceivePriority(aReceivePriority)
    , mChannel(0)
{
    OT_ASSERT((kPriorityReceiveMin <= aReceivePriority) && (aReceivePriority <= kPriorityReceiveMax));
}

Error RadioInterface::Sleep(void) { return Get<RadioScheduler>().Sleep(*this); }

Error RadioInterface::Receive(uint8_t aChannel)
{
    return Get<RadioScheduler>().Receive(*this, mReceivePriority, aChannel);
}

Error RadioInterface::ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    return Get<RadioScheduler>().ReceiveAt(aChannel, aStart, aDuration);
}

void RadioInterface::SetStateAndPriority(State aState, Priority aPriority)
{
    mState       = aState;
    mCurPriority = aPriority;
}

const char *RadioInterface::StateToString(State aState) const
{
    static const char *const kStateStrings[] = {
        "Disabled",   // (0) kStateDisabled
        "Enabled",    // (1) kStateEnabled
        "Sleep",      // (2) kStateSleep
        "Receive",    // (3) kStateReceive
        "Transmit",   // (4) kStateTransmit
        "EnergyScan", // (5) kStateEnergyScan
    };

    struct StateValueChecker
    {
        InitEnumValidatorCounter();

        ValidateNextEnum(kStateDisabled);
        ValidateNextEnum(kStateEnabled);
        ValidateNextEnum(kStateSleep);
        ValidateNextEnum(kStateReceive);
        ValidateNextEnum(kStateTransmit);
        ValidateNextEnum(kStateEnergyScan);
    };

    return kStateStrings[aState];
}

RadioInterface::InfoString RadioInterface::ToString(void) const
{
    InfoString string;

    if (mReceivePriority == kPriorityReceiveMac)
    {
        string.Append("Mac");
    }
    else if (mReceivePriority == kPriorityReceiveCsl)
    {
        string.Append("Csl");
    }
    else if (mReceivePriority == kPriorityReceiveWed)
    {
        string.Append("Wed");
    }

    string.Append("state=%s,prio=%u,ch=%u", StateToString(mState), mCurPriority, mChannel);

    return string;
}

RadioScheduler::RadioScheduler(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mCallbacks(aInstance)
    , mRadios
{
    {aInstance, RadioInterface::kPriorityReceiveMac},
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        {aInstance, RadioInterface::kPriorityReceiveCsl},
#endif
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
        {aInstance, RadioInterface::kPriorityReceiveWed},
#endif
}
{
}

Error RadioScheduler::Sleep(RadioInterface &aRadio)
{
    aRadio.SetStateAndPriority(RadioInterface::kStateSleep, RadioInterface::kPrioritySleep);
    ReceiveOrSleep();

    return kErrorNone;
}

Error RadioScheduler::Receive(RadioInterface &aRadio, RadioInterface::Priority aReceivePriority, uint8_t aChannel)
{
    aRadio.SetStateAndPriority(RadioInterface::kStateReceive, aReceivePriority);
    aRadio.SetChannel(aChannel);
    ReceiveOrSleep();

    return kErrorNone;
}

Error RadioScheduler::Enable(void)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Enable());

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        mRadios[i].SetStateAndPriority(RadioInterface::kStateEnabled, RadioInterface::kPriorityMin);
    }

exit:
    return error;
}

Error RadioScheduler::Disable(void)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Disable());

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        mRadios[i].SetStateAndPriority(RadioInterface::kStateDisabled, RadioInterface::kPriorityMax);
    }

exit:
    return error;
}

void RadioScheduler::ReceiveOrSleep(void)
{
    uint8_t index       = kNumRadios;
    uint8_t maxPriority = RadioInterface::kPriorityMin;

    // Find the radio in the 'sleep' or 'receive' state with the highest priority.
    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        if (mRadios[i].mCurPriority > maxPriority)
        {
            index       = i;
            maxPriority = mRadios[index].mCurPriority;
        }
    }

    VerifyOrExit(index < kNumRadios);
    VerifyOrExit((mRadios[index].mState == RadioInterface::kStateSleep) ||
                 (mRadios[index].mState == RadioInterface::kStateReceive));

    if (mRadios[index].mState == RadioInterface::kStateSleep)
    {
        IgnoreError(Get<Radio>().Sleep());
    }
    else if (mRadios[index].mState == RadioInterface::kStateReceive)
    {
        IgnoreError(Get<Radio>().Receive(mRadios[index].mChannel));
    }
    else
    {
        OT_ASSERT(false);
    }

exit:
    return;
}

Error RadioScheduler::ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    return Get<Radio>().ReceiveAt(aChannel, aStart, aDuration);
}

Error RadioScheduler::Transmit(Mac::TxFrame &aFrame)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Transmit(aFrame));
    mRadios[kRadioMac].SetStateAndPriority(RadioInterface::kStateTransmit, RadioInterface::kPriorityTransmit);

exit:
    return error;
}

Error RadioScheduler::EnergyScan(uint8_t aScanChannel, uint16_t aScanDuration)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().EnergyScan(aScanChannel, aScanDuration));
    mRadios[kRadioMac].SetStateAndPriority(RadioInterface::kStateEnergyScan, RadioInterface::kPriorityEnergyScan);

exit:
    return error;
}

void RadioScheduler::EnergyScanDone(void)
{
    mRadios[kRadioMac].SetStateAndPriority(RadioInterface::kStateEnabled, RadioInterface::kPriorityMin);
    ReceiveOrSleep();
}

void RadioScheduler::TransmitDone(void)
{
    mRadios[kRadioMac].SetStateAndPriority(RadioInterface::kStateEnabled, RadioInterface::kPriorityMin);
    ReceiveOrSleep();
}

void RadioScheduler::Callbacks::HandleEnergyScanDone(int8_t aMaxRssi)
{
    Get<RadioScheduler>().EnergyScanDone();
    Get<Mac::SubMac>().HandleEnergyScanDone(aMaxRssi);
}

void RadioScheduler::Callbacks::HandleTransmitDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError)
{
    Get<RadioScheduler>().TransmitDone();
    Get<Mac::SubMac>().HandleTransmitDone(aFrame, aAckFrame, aError);
}
} // namespace ot
#endif // OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
