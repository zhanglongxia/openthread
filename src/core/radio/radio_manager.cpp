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
 *   This file implements the radio manager.
 */

#include "radio_manager.hpp"

#include "common/code_utils.hpp"
#include "common/locator.hpp"
#include "common/log.hpp"
#include "instance/instance.hpp"
#include "radio/radio.hpp"

namespace ot {
RegisterLogModule("RadioManager");

RadioInterface::RadioInterface(Instance &aInstance, uint8_t aReceivePriority)
    : InstanceLocator(aInstance)
    , mState(kStateDisabled)
    , mPriority(kPriorityMax)
    , mReceivePriority(aReceivePriority)
    , mChannel(0)

{
    OT_ASSERT((kPriorityRxMin <= aReceivePriority) && (aReceivePriority <= kPriorityRxMax));
}

Error RadioInterface::Sleep(void) { return Get<RadioManager>().Sleep(*this); }

Error RadioInterface::Receive(uint8_t aChannel)
{
    return Get<RadioManager>().Receive(*this, mReceivePriority, aChannel);
}

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
Error RadioInterface::ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    return Get<RadioManager>().ReceiveAt(aChannel, aStart, aDuration);
}
#endif

void RadioInterface::Set(State aState, uint8_t aPriority)
{
    mState    = aState;
    mPriority = aPriority;
}

void RadioInterface::Set(State aState, uint8_t aPriority, uint8_t aChannel)
{
    mState    = aState;
    mPriority = aPriority;
    mChannel  = aChannel;
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

    if (mReceivePriority == kPriorityRxBase)
    {
        string.Append("Mac");
    }
    else if (mReceivePriority == kPriorityRxCsl)
    {
        string.Append("Csl");
    }
    else if (mReceivePriority == kPriorityRxWed)
    {
        string.Append("Wed");
    }

    string.Append("[state=%s,prio=%u,ch=%u]", StateToString(mState), mPriority, mChannel);

    return string;
}

RadioManager::RadioManager(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mRadios
{
    {aInstance, RadioInterface::kPriorityRxBase},
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        {aInstance, RadioInterface::kPriorityRxCsl},
#endif
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
        {aInstance, RadioInterface::kPriorityRxWed},
#endif
}
{
}

Error RadioManager::Sleep(RadioInterface &aRadio)
{
    aRadio.Set(RadioInterface::kStateSleep, RadioInterface::kPrioritySleep);
    ReceiveOrSleep();

    return kErrorNone;
}

Error RadioManager::Receive(RadioInterface &aRadio, uint8_t aReceivePriority, uint8_t aChannel)
{
    aRadio.Set(RadioInterface::kStateReceive, aReceivePriority, aChannel);
    ReceiveOrSleep();

    return kErrorNone;
}

Error RadioManager::Enable(void)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Enable());

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        mRadios[i].Set(RadioInterface::kStateEnabled, RadioInterface::kPriorityMin);
    }

exit:
    return error;
}

Error RadioManager::Disable(void)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Disable());

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        mRadios[i].Set(RadioInterface::kStateDisabled, RadioInterface::kPriorityMax);
    }

exit:
    return error;
}

void RadioManager::LogRadios(const char *aName)
{
    char         buf[512];
    StringWriter writer(buf, sizeof(buf));

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        writer.Append("%s,", mRadios[i].ToString().AsCString());
    }

    LogCrit("%s: Radios: %s", aName, buf);
}

void RadioManager::ReceiveOrSleep(void)
{
    uint8_t index       = kNumRadios;
    uint8_t maxPriority = RadioInterface::kPriorityMin;

    for (uint8_t i = 0; i < kNumRadios; i++)
    {
        if (mRadios[i].mPriority > maxPriority)
        {
            index       = i;
            maxPriority = mRadios[index].mPriority;
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

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
Error RadioManager::ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    return Get<Radio>().ReceiveAt(aChannel, aStart, aDuration);
}
#endif

Error RadioManager::Transmit(Mac::TxFrame &aFrame)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().Transmit(aFrame));
    mRadios[kRadioMac].Set(RadioInterface::kStateTransmit, RadioInterface::kPriorityTx);

exit:
    return error;
}

Error RadioManager::EnergyScan(uint8_t aScanChannel, uint16_t aScanDuration)
{
    Error error;

    SuccessOrExit(error = Get<Radio>().EnergyScan(aScanChannel, aScanDuration));
    mRadios[kRadioMac].Set(RadioInterface::kStateEnergyScan, RadioInterface::kPriorityEnergyScan);

exit:
    return error;
}

void RadioManager::HandleEnergyScanDone(int8_t aMaxRssi)
{
    mRadios[kRadioMac].Set(RadioInterface::kStateReceive, RadioInterface::kPriorityRxBase);
    ReceiveOrSleep();

    Get<Mac::SubMac>().HandleEnergyScanDone(aMaxRssi);
}

void RadioManager::HandleTransmitDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError)
{
    mRadios[kRadioMac].Set(RadioInterface::kStateReceive, RadioInterface::kPriorityRxBase);
    ReceiveOrSleep();

    Get<Mac::SubMac>().HandleTransmitDone(aFrame, aAckFrame, aError);
}
} // namespace ot
