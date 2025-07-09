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
 *  This file defines the OpenThread unstable IEEE 802.15.4 Link Layer API.
 */
#ifndef OPENTHREAD_UNSTABLE_LINK_H_
#define OPENTHREAD_UNSTABLE_LINK_H_

#include <openthread/link.h>
#include <openthread/platform/radio.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @addtogroup unstable-api-link
 *
 * @brief
 *   This module includes unstable functions that control link-layer configuration.
 *
 * @{
 */

/**
 * Add a wake-up identifier to wake-up identifier table.
 *
 * @param[in]  aInstance    The OpenThread instance structure.
 * @param[in]  aWakeupId    The wake-up identifier to be added.
 *
 * @retval OT_ERROR_NONE      Successfully added wake-up identifier to the wake-up identifier table.
 * @retval OT_ERROR_NO_BUFS   No available entry in the wake-up identifier table.
 */
otError otLinkAddWakeupId(otInstance *aInstance, uint64_t aWakeupId);

/**
 * Remove a wake-up identifier from the wake-up identifier table.
 *
 * @param[in]  aInstance    The OpenThread instance structure.
 * @param[in]  aWakeupId    The wake-up identifier to be removed.
 *
 * @retval OT_ERROR_NONE        Successfully removed the wake-up identifier from the wake-up identifier table.
 * @retval OT_ERROR_NOT_FOUND   The wake-up identifier was not in wake-up identifier table.
 */
otError otLinkRemoveWakeupId(otInstance *aInstance, uint64_t aWakeupId);

/**
 * Clear all wake-up identifiers from the wake-up identifier table.
 *
 * @param[in]  aInstance   The OpenThread instance structure.
 */
void otLinkClearWakeupIds(otInstance *aInstance);
/**
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENTHREAD_UNSTABLE_LINK_H_
