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
 *   This file includes definitions for the radio scheduler.
 */

#ifndef RADIO_SCHEDULER_HPP_
#define RADIO_SCHEDULER_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
#include <openthread/platform/radio.h>

#include "common/clearable.hpp"
#include "common/locator.hpp"
#include "radio/radio.hpp"

namespace ot {

/**
 * @addtogroup core-radio
 *
 * @brief
 *   This module includes definitions for the radio scheduler
 *
 * @{
 *
 */

class RadioScheduler;

/**
 * Represents an radio interface.
 *
 */
class RadioInterface : public InstanceLocator, private NonCopyable
{
    friend class Instance;
    friend class RadioScheduler;

public:
    static constexpr uint16_t kInfoStringSize = 60; ///< Max chars for the info string (`ToString()`).

    /**
     * Defines the fixed-length `String` object returned from `ToString()`.
     */
    typedef String<kInfoStringSize> InfoString;

    /**
     * Transitions the radio from Receive to Sleep (turn off the radio).
     *
     * @retval kErrorNone          Successfully transitioned to Sleep.
     * @retval kErrorBusy          The radio was transmitting.
     * @retval kErrorInvalidState  The radio was disabled.
     */
    Error Sleep(void);

    /**
     * Transitions the radio from Sleep to Receive (turn on the radio).
     *
     * @param[in]  aChannel   The channel to use for receiving.
     *
     * @retval kErrorNone          Successfully transitioned to Receive.
     * @retval kErrorInvalidState  The radio was disabled or transmitting.
     */
    Error Receive(uint8_t aChannel);

    /**
     * Schedules a radio reception window at a specific time and duration.
     *
     * @param[in]  aChannel   The radio channel on which to receive.
     * @param[in]  aStart     The receive window start time, in microseconds.
     * @param[in]  aDuration  The receive window duration, in microseconds.
     *
     * @retval kErrorNone    Successfully scheduled receive window.
     * @retval kErrorFailed  The receive window could not be scheduled.
     */
    Error ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration);

private:
    enum State : uint8_t
    {
        kStateDisabled   = 0,
        kStateEnabled    = 1,
        kStateSleep      = 2,
        kStateReceive    = 3,
        kStateTransmit   = 4,
        kStateEnergyScan = 5,
    };

    enum Priority : uint8_t
    {
        kPriorityMin        = 0,
        kPrioritySleep      = 1,
        kPriorityReceiveMin = 2,
        kPriorityReceiveWed = 7,
        kPriorityReceiveCsl = 9,
        kPriorityReceiveMac = 11,
        kPriorityReceiveMax = 13,
        kPriorityTransmit   = 14,
        kPriorityEnergyScan = 14,
        kPriorityMax        = 15,
    };

    RadioInterface(Instance &aInstance, Priority aReceivePriority);

    void        SetStateAndPriority(State aState, Priority aPriority);
    void        SetChannel(uint8_t aChannel) { mChannel = aChannel; }
    const char *StateToString(State aState) const;
    InfoString  ToString(void) const;

    State    mState;
    Priority mCurPriority;
    Priority mReceivePriority;
    uint8_t  mChannel;
};

/**
 * Represents a module to automatically schedule sleep and receive operations.
 */
class RadioScheduler : public InstanceLocator, private NonCopyable
{
    friend class Instance;
    friend class RadioInterface;

public:
    explicit RadioScheduler(Instance &aInstance);

    /**
     * Enables the radio.
     *
     * @retval kErrorNone     Successfully enabled.
     * @retval kErrorFailed   The radio could not be enabled.
     */
    Error Enable(void);

    /**
     * Disables the radio.
     *
     * @retval kErrorNone           Successfully transitioned to Disabled.
     * @retval kErrorInvalidState   The radio was not in sleep state.
     */
    Error Disable(void);

    /**
     * Starts the transmit sequence on the radio.
     *
     * The caller must form the IEEE 802.15.4 frame in the buffer provided by `GetTransmitBuffer()` before
     * requesting transmission.  The channel and transmit power are also included in the frame.
     *
     * @param[in] aFrame     A reference to the frame to be transmitted.
     *
     * @retval kErrorNone          Successfully transitioned to Transmit.
     * @retval kErrorInvalidState  The radio was not in the Receive state.
     */
    Error Transmit(Mac::TxFrame &aFrame);

    /**
     * Begins the energy scan sequence on the radio.
     *
     * Is used when radio provides OT_RADIO_CAPS_ENERGY_SCAN capability.
     *
     * @param[in] aScanChannel   The channel to perform the energy scan on.
     * @param[in] aScanDuration  The duration, in milliseconds, for the channel to be scanned.
     *
     * @retval kErrorNone            Successfully started scanning the channel.
     * @retval kErrorBusy            The radio is performing energy scanning.
     * @retval kErrorNotImplemented  The radio doesn't support energy scanning.
     */
    Error EnergyScan(uint8_t aScanChannel, uint16_t aScanDuration);

    /**
     * Returns a reference to the radio interface for the MAC layer.
     *
     * @returns A reference to the radio interface.
     */
    RadioInterface &GetMacRadio(void) { return mRadios[kRadioMac]; }

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    /**
     * Returns a reference to the radio interface for the CSL receiver.
     *
     * @returns A reference to the radio interface.
     */
    RadioInterface &GetCslRadio(void) { return mRadios[kRadioCsl]; }
#endif

#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
    /**
     * Returns a reference to the radio interface for the WED.
     *
     * @returns A reference to the radio interface.
     */
    RadioInterface &GetWedRadio(void) { return mRadios[kRadioWed]; }
#endif

    /**
     * Defines the callbacks from `RadioScheduler`.
     */
    class Callbacks : public InstanceLocator
    {
        friend RadioScheduler;

    public:
        /**
         * This callback method handles a "Transmit Done" event from `Radio::Callbacks`.
         *
         * @param[in]  aFrame     The frame that was transmitted.
         * @param[in]  aAckFrame  A pointer to the ACK frame, `nullptr` if no ACK was received.
         * @param[in]  aError     kErrorNone when the frame was transmitted,
         *                        kErrorNoAck when the frame was transmitted but no ACK was received,
         *                        kErrorChannelAccessFailure tx could not take place due to activity on the
         *                        channel, kErrorAbort when transmission was aborted for other reasons.
         */
        void HandleTransmitDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError);

        /**
         * This callback method handles "Energy Scan Done" event from `Radio::Callbacks`.
         *
         * @param[in]  aMaxRssi  The maximum RSSI encountered on the scanned channel.
         */
        void HandleEnergyScanDone(int8_t aMaxRssi);

    private:
        explicit Callbacks(Instance &aInstance)
            : InstanceLocator(aInstance)
        {
        }
    };

private:
    Error Sleep(RadioInterface &aRadio);
    Error Receive(RadioInterface &aRadio, RadioInterface::Priority aReceivePriority, uint8_t aChannel);
    Error ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration);
    void  ReceiveOrSleep(void);
    void  TransmitDone(void);
    void  EnergyScanDone(void);

    enum : uint8_t
    {
        kRadioMac = 0,
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        kRadioCsl,
#endif
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
        kRadioWed,
#endif
        kNumRadios,
    };

    Callbacks      mCallbacks;
    RadioInterface mRadios[kNumRadios];
};

/**
 * @}
 *
 */

} // namespace ot
#endif // OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
#endif // RADIO_SCHEDULER_HPP_
