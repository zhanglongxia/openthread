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
 *   This file implements the Wake-up End Device of the subset of IEEE 802.15.4 MAC primitives.
 */

#include "sub_mac.hpp"

#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE

#include "instance/instance.hpp"

namespace ot {
namespace Mac {

RegisterLogModule("SubMac");

void SubMac::WedInit(void)
{
    mIsRx                 = false;
    mWakeupListenInterval = 0;
    mWedTimer.Stop();
}

void SubMac::UpdateWakeupListening(bool aEnable, uint32_t aInterval, uint32_t aDuration, uint8_t aChannel)
{
    LogInfo("UpdateWakeupListening() aEnable=%u", aEnable);

    mWakeupListenInterval = aInterval;
    mWakeupListenDuration = aDuration;
    mWakeupChannel        = aChannel;
    mWedTimer.Stop();

    if (aEnable)
    {
        mIsRx               = true;
        mWedSampleTime      = TimerMicro::GetNow() + kCslReceiveTimeAhead - mWakeupListenInterval;
        mWedSampleTimeRadio = Get<Radio>().GetNow() + kCslReceiveTimeAhead - mWakeupListenInterval;

        HandleWedTimer();
    }
}

void SubMac::HandleWedTimer(Timer &aTimer) { aTimer.Get<SubMac>().HandleWedTimer(); }

void SubMac::HandleReceiveAt(void)
{
    /**
     *   ------+-------+------------------+-------+------------------+-------
     *  Now  SamTime0                   SamTime1
     *  Now  RadioTime0                 RadioTime1   |
     *                                             FireAt(SamTime1+Dur+After)
     *                                 FireAt(RadioTime1, Dur)
     */
    mWedSampleTime += mWakeupListenInterval;
    mWedSampleTimeRadio += mWakeupListenInterval;
    mWedTimer.FireAt(mWedSampleTime + mWakeupListenDuration + kWedReceiveTimeAfter);

    if (mState != kStateDisabled)
    {
        IgnoreError(
            Get<Radio>().ReceiveAt(mWakeupChannel, static_cast<uint32_t>(mWedSampleTimeRadio), mWakeupListenDuration));
    }
}

void SubMac::HandleReceiveAndSleep(void)
{
    /**
     *   ------+-----------+----------------------+-------+------------------+-------
     *  Now  SamTimeRx
     *         | DelayRx   |
     *                FireAt(SamTimeRx, DelayRx)
     *
     *                    Now
     *                  SamTimeSleep
     *                     |   DelaySleep         |
     *                                       FireAt(SamTimeSleep, DelaySleep)
     */
    uint32_t interval = mIsRx ? mWakeupListenDuration : (mWakeupListenInterval - mWakeupListenDuration);
    int32_t  delay    = mIsRx ? kMinReceiveOnAfter : -kMinReceiveOnAhead;

    mWedSampleTime += interval;
    mWedTimer.FireAt(mWedSampleTime + delay);

    if (mState != kStateDisabled)
    {
        if (mIsRx)
        {
            LogInfo("Rx(): ch=%u", mWakeupChannel);
            IgnoreError(Get<Radio>().Receive(mWakeupChannel));
        }
        else
        {
            LogInfo("Sleep()");
            // IgnoreError(Get<Radio>().Sleep());
        }
    }

    mIsRx = !mIsRx;
}

void SubMac::HandleWedTimer(void)
{
    if (RadioSupportsReceiveTiming())
    {
        HandleReceiveAt();
    }
    else
    {
        HandleReceiveAndSleep();
    }
}

} // namespace Mac
} // namespace ot

#endif // OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
