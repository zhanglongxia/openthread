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

#include "common/locator.hpp"
#include "common/locator_getters.hpp"
#include "common/log.hpp"
#include "instance/instance.hpp"
#include "radio/radio.hpp"

namespace ot {
RegisterLogModule("RadioManager");

Error RadioManager::Register(uint8_t aId, uint8_t aPriority) {}

Error RadioManager::Enable(void)
{
    mActions[kIdBase].Init(kStateEnabled, kPriorityEnabled, 0);
    mActions[kIdCsl].Init(kStateEnabled, kPriorityEnabled, 0);
    mActions[kIdWakeUp].Init(kStateEnabled, kPriorityEnabled, 0);

    LogCrit("Enabled()");
    return Get<Radio>().Enable();
}

Error RadioManager::Disable(void)
{
    mActions[kIdBase].Init(kStateDisabled, kPriorityDisabled, 0);
    mActions[kIdCsl].Init(kStateDisabled, kPriorityDisabled, 0);
    mActions[kIdWakeUp].Init(kStateDisabled, kPriorityDisabled, 0);

    LogCrit("Disabled()");
    return Get<Radio>().Disable();
}

const char *RadioManager::ActionsToString(void)
{
    static char buf[100] = {0};
    char       *start    = buf;
    char       *end      = buf + sizeof(buf);

    for (uint8_t i = 0; i < kNumIds; i++)
    {
        start += snprintf(start, end - start, "{state: %s, prio:%u, ch:%u} ", StateToString(mActions[i].mState),
                          mActions[i].mPriority, mActions[i].mChannel);
    }
    start[0] = '\0';

    return buf;
}

void RadioManager::SleepOrReceive(void)
{
    uint8_t index       = kNumIds;
    uint8_t maxPriority = 0;

    LogCrit("SleepOrReceive() : %s", ActionsToString());

    for (uint8_t i = 0; i < kNumIds; i++)
    {
        if (mActions[i].mPriority > maxPriority)
        {
            index       = i;
            maxPriority = mActions[i].mPriority;
        }
    }

    VerifyOrExit(index != kNumIds);
    VerifyOrExit((mActions[index].mState == kStateSleep) || (mActions[index].mState == kStateReceive));

    if (mActions[index].mState == kStateSleep)
    {
        LogCrit("SleepOrReceive() -> Sleep()");
        IgnoreError(Get<Radio>().Sleep());
    }
    else if (mActions[index].mState == kStateReceive)
    {
        LogCrit("SleepOrReceive() -> Receive(%u)", mActions[index].mChannel);
        IgnoreError(Get<Radio>().Receive(mActions[index].mChannel));
    }
    else
    {
        LogCrit("SleepOrReceive() -> None 1");
    }

exit:
    if (index == kNumIds)
    {
        LogCrit("SleepOrReceive() -> None");
    }
    return;
}

Error RadioManager::Sleep(Id aId, Priority aPriority)
{
    LogCrit("Sleep() Id=%u", aId);
    OT_ASSERT(aId < kNumIds);

    mActions[aId].Init(kStateSleep, aPriority);
    SleepOrReceive();

    return kErrorNone;
}

Error RadioManager::Receive(Id aId, Priority aPriority, uint8_t aChannel)
{
    OT_ASSERT(aId < kNumIds);
    LogCrit("Receive() Id=%u", aId);

    mActions[aId].Init(kStateReceive, aPriority, aChannel);
    SleepOrReceive();

    return kErrorNone;
}

Error RadioManager::ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    LogCrit("ReceiveAt()");
    VerifyOrExit((mActions[kIdBase].mState != kStateDisabled) && (mActions[kIdBase].mState != kStateReceive));
    IgnoreError(Get<Radio>().ReceiveAt(aChannel, aStart, aDuration));
    mActions[kIdCsl].Init(kStateReceiveAt, kPriorityCslRxAt, aChannel);

exit:
    return kErrorNone;
}

Error RadioManager::Transmit(Mac::TxFrame &aFrame)
{
    LogCrit("Transmit()");
    mActions[kIdBase].Init(kStateTransmit, kPriorityTx);
    return Get<Radio>().Transmit(aFrame);
}

void RadioManager::HandleTransmitDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError)
{
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aAckFrame);
    OT_UNUSED_VARIABLE(aError);

    LogCrit("TransmitDone()");
    mActions[kIdBase].Init(kStateEnabled, kPriorityEnabled);

    SleepOrReceive();
}

const char *RadioManager::StateToString(State aState)
{
    static const char *const kStateStrings[] = {
        "kStateDisabled",  // (0) kStateDisabled
        "kStateEnabled",   // (1) kStateEnabled
        "kStateSleep",     // (2) kStateSleep
        "kStateReceive",   // (3) kStateReceive
        "kStateTransmit",  // (4) kStateTransmit
        "kStateReceiveAt", // (5) kStateReceiveAt
    };

    return kStateStrings[aState];
}
} // namespace ot
