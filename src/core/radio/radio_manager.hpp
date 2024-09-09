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
 *   This file includes definitions for the radio manager.
 */

#ifndef RADIO_MANAGER_HPP_
#define RADIO_MANAGER_HPP_

#include "openthread-core-config.h"

#include <openthread/platform/radio.h>

#include "common/clearable.hpp"
#include "common/locator.hpp"
#include "radio/radio.hpp"

namespace ot {

/**
 * @addtogroup core-mac
 *
 * @brief
 *   This module includes definitions for the radio manager
 *
 * @{
 *
 */

/**
 * Represents an OpenThread radio manager.
 *
 */
class RadioManager : public InstanceLocator, private NonCopyable
{
    friend class Instance;

public:
    explicit RadioManager(Instance &aInstance)
        : InstanceLocator(aInstance)
    {
    }

    Error Enable(void);
    Error Disable(void);
    Error Sleep(void) { return Sleep(kIdBase, kPrioritySleep); }
    Error Receive(uint8_t aChannel) { return Receive(kIdBase, kPriorityRx, aChannel); }
    Error Transmit(Mac::TxFrame &aFrame);
    void  HandleTransmitDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError);

    Error CslSleep(void) { return Sleep(kIdCsl, kPriorityCslSleep); }
    Error CslReceive(uint8_t aChannel) { return Receive(kIdCsl, kPriorityCslRx, aChannel); }
    Error ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration);

private:
    enum Id : uint8_t
    {
        kIdBase = 0,
        kIdCsl  = 1,
        kNumIds = 2,
    };

    enum Priority : uint8_t
    {
        kPriorityDisabled = 15,
        kPriorityEnabled  = 0,
        kPriorityTx       = 12,
        kPriorityRx       = 12,
        kPrioritySleep    = 4,

        kPriorityCslRx    = 8,
        kPriorityCslRxAt  = 2,
        kPriorityCslSleep = 4,
    };

    enum State : uint8_t
    {
        kStateDisabled  = 0,
        kStateEnabled   = 1,
        kStateSleep     = 2,
        kStateReceive   = 3,
        kStateTransmit  = 4,
        kStateReceiveAt = 5,
    };

    class Action
    {
    public:
        void Init(State aState, Priority aPriority)
        {
            mState    = aState;
            mPriority = aPriority;
        }

        void Init(State aState, Priority aPriority, uint8_t aChannel)
        {
            mState    = aState;
            mPriority = aPriority;
            mChannel  = aChannel;
        }

        State   mState : 4;
        uint8_t mPriority : 4;
        uint8_t mChannel;
    };

    Error       Sleep(Id aId, Priority aPriority);
    Error       Receive(Id aId, Priority aPriority, uint8_t aChannel);
    const char *StateToString(State aState);
    const char *ActionsToString(void);
    void        SleepOrReceive(void);

    Action mActions[kNumIds];
};

/**
 * @}
 *
 */

} // namespace ot
#endif // RADIO_MANAGER_HPP_
