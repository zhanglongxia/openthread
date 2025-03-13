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
 * @brief
 *  This file defines the OpenThread peer-to-peer API.
 */

#ifndef OPENTHREAD_P2P_H_
#define OPENTHREAD_P2P_H_

#include <openthread/link.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Informs the application about the result of connecting to the Wake-up End Device.
 *
 * @param[in] aError   OT_ERROR_NONE    Indicates that at least one peer-to-peer link has been established with WEDs.
 *                     OT_ERROR_FAILED  Indicates that the WED has not received a wake-up frame, or it
 *                                      has failed to connect to the WC.
 * @param[in] aContext A pointer to application-specific context.
 */
typedef void (*otP2pConnectedCallback)(otError aError, void *aContext);

/**
 * Attempts to establish peer-to-peer links with WEDs.
 *
 * @param[in] aInstance          A pointer to an OpenThread instance.
 * @param[in] aWakeupAddress     A pointer to wake-up address.
 * @param[in] aWakeupIntervalUs  Interval between consecutive wake-up frames (in microseconds).
 * @param[in] aWakeupDurationMs  Duration of the wake-up sequence (in milliseconds).
 * @param[in] aCallback          A pointer to function that is called when the peer-to-peer link succeeds or
 *                               fails.
 * @param[in] aCallbackContext   A pointer to callback application-specific context.
 *
 * @retval OT_ERROR_NONE          Successfully started the wake-up.
 * @retval OT_ERROR_INVALID_STATE Another attachment request is still in progress.
 * @retval OT_ERROR_INVALID_ARGS  The wake-up address, wake-up interval or duration are invalid.
 */
otError otP2pConnect(otInstance            *aInstance,
                     const otWakeupAddress *aWakeupAddress,
                     uint16_t               mWakeupIntervalUs,
                     uint16_t               mWakeupDurationMs,
                     otP2pConnectedCallback aCallback,
                     void                  *aContext);

/**
 * Tear down the peer-to-peer link.
 *
 * @param[in] aInstance      A pointer to an OpenThread instance.
 * @param[in] aExtAddress    A pointer to the peer's Extended Address.
 *
 * @retval OT_ERROR_NONE          Successfully started the wake-up.
 * @retval OT_ERROR_NOT_FOUND     The peer-to-peer link identified by the @p aExtAddress was not found.
 */
otError otP2pDisconnect(otInstance *aInstance, const otExtAddress *aExtAddress);

/**
 * Defines events of the peer-to-peer link.
 */
typedef enum
{
    OT_P2P_EVENT_WED_CONNECTED    = 0, ///< The device is connected to the WED.
    OT_P2P_EVENT_WED_DISCONNECTED = 1, ///< The device is disconnected from the WC.
    OT_P2P_EVENT_WC_CONNECTED     = 2, ///< The device is connected to the WC.
    OT_P2P_EVENT_WC_DISCONNECTED  = 3, ///< The device is disconnected from the WC.
} otP2pEvent;

/**
 * Callback function pointer to signal events of the peer-to-peer link.
 *
 * @param[in] aEvent      The peer-to-peer link event.
 * @param[in] aExtAddress A pointer to the Extended Address of the peer of the peer-to-peer link.
 * @param[in] aContext    A user context pointer.
 */
typedef void (*otP2pEventCallback)(otP2pEvent aEvent, const otExtAddress *aExtAddress, void *aContext);

/**
 * Sets the callback function to notify event changes of peer-to-peer links.
 *
 * A subsequent call to this function will replace any previously set callback.
 *
 * @param[in] aInstance    The OpenThread instance.
 * @param[in] aCallback    The callback function pointer.
 * @param[in] aContext     A user context pointer.
 */
void otP2pSetEventCallback(otInstance *aInstance, otP2pEventCallback aCallback, void *aContext);

/**
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENTHREAD_P2P_H_
