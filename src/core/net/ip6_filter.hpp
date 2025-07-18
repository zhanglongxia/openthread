/*
 *  Copyright (c) 2016, The OpenThread Authors.
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
 *   This file includes definitions for IPv6 datagram filtering.
 */

#ifndef IP6_FILTER_HPP_
#define IP6_FILTER_HPP_

#include "openthread-core-config.h"

#include "common/array.hpp"
#include "common/locator.hpp"
#include "common/message.hpp"
#include "common/non_copyable.hpp"

namespace ot {
namespace Ip6 {

/**
 * @addtogroup core-ipv6
 *
 * @brief
 *   This module includes definitions for IPv6 datagram filtering.
 *
 * @{
 */

/**
 * Implements an IPv6 datagram filter.
 */
class Filter : public InstanceLocator, private NonCopyable
{
public:
    /**
     * Initializes the Filter object.
     *
     * @param[in]  aInstance  A reference to the OpenThread instance.
     */
    explicit Filter(Instance &aInstance)
        : InstanceLocator(aInstance)
    {
    }

    /**
     * Applies the filter to an IPv6 datagram to determine if it should be dropped.
     *
     * @param[in]  aMessage  The IPv6 datagram to process.
     *
     * @retval kErrorNone  The message is not filtered and should be accepted.
     * @retval kErrorDrop  The message matches the filter criteria and should be dropped.
     */
    Error Apply(const Message &aMessage) const;

    /**
     * Adds a port to the allowed unsecured port list.
     *
     * @param[in]  aPort  The port value.
     *
     * @retval kErrorNone         The port was successfully added to the allowed unsecure port list.
     * @retval kErrorInvalidArgs  The port is invalid (value 0 is reserved for internal use).
     * @retval kErrorNoBufs       The unsecure port list is full.
     */
    Error AddUnsecurePort(uint16_t aPort) { return UpdateUnsecurePorts(kAdd, aPort); }

    /**
     * Removes a port from the allowed unsecure port list.
     *
     * @param[in]  aPort  The port value.
     *
     * @retval kErrorNone         The port was successfully removed from the allowed unsecure port list.
     * @retval kErrorInvalidArgs  The port is invalid (value 0 is reserved for internal use).
     * @retval kErrorNotFound     The port was not found in the unsecure port list.
     */
    Error RemoveUnsecurePort(uint16_t aPort) { return UpdateUnsecurePorts(kRemove, aPort); }

    /**
     * Checks whether a port is in the unsecure port list.
     *
     * @param[in]  aPort  The port value.
     *
     * @returns Whether the given port is in the unsecure port list.
     */
    bool IsUnsecurePort(uint16_t aPort) { return mUnsecurePorts.Contains(aPort); }

    /**
     * Removes all ports from the allowed unsecure port list.
     */
    void RemoveAllUnsecurePorts(void) { mUnsecurePorts.Clear(); }

    /**
     * Returns a pointer to the unsecure port list.
     *
     * @note Port value 0 is used to indicate an invalid entry.
     *
     * @param[out]  aNumEntries  The number of entries in the list.
     *
     * @returns A pointer to the unsecure port list.
     */
    const uint16_t *GetUnsecurePorts(uint8_t &aNumEntries) const
    {
        aNumEntries = mUnsecurePorts.GetLength();

        return &mUnsecurePorts[0];
    }

private:
    static constexpr uint16_t kMaxUnsecurePorts = 2;

    enum Action : uint8_t
    {
        kAdd,
        kRemove,
    };

    Error UpdateUnsecurePorts(Action aAction, uint16_t aPort);

    Array<uint16_t, kMaxUnsecurePorts> mUnsecurePorts;
};

} // namespace Ip6
} // namespace ot

#endif // IP6_FILTER_HPP_
