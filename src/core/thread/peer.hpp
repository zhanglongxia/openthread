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
 *   This file includes definitions for a Thread P2P `Peer`.
 */

#ifndef PEER_HPP_
#define PEER_HPP_

#include "openthread-core-config.h"

#include "common/bit_set.hpp"
#include "thread/neighbor.hpp"

namespace ot {

#if OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE

/**
 * Represents a P2P Peer.
 */
class Peer : public CslNeighbor
{
public:
    static constexpr uint8_t kMaxRequestTlvs = 6;

    /**
     * Initializes the `Peer` object.
     *
     * @param[in] aInstance  A reference to OpenThread instance.
     */
    void Init(Instance &aInstance) { Neighbor::Init(aInstance); }

    /**
     * Clears the child entry.
     */
    void Clear(void);

    /**
     * Sets the device mode flags.
     *
     * @param[in]  aMode  The device mode flags.
     */
    void SetDeviceMode(Mle::DeviceMode aMode);

    /**
     * Gets an array of registered IPv6 address entries by the child.
     *
     * The array does not include the mesh-local EID. The ML-EID can retrieved using  `GetMeshLocalIp6Address()`.
     *
     * @returns The array of registered IPv6 addresses by the child.
     */
    void GetLinkLocalIp6Address(Ip6::Address &aIp6Address) const { aIp6Address.SetToLinkLocalAddress(GetExtAddress()); }

    /**
     * Gets the network data version.
     *
     * @returns The network data version.
     */
    uint8_t GetNetworkDataVersion(void) const { return mNetworkDataVersion; }

    /**
     * Sets the network data version.
     *
     * @param[in]  aVersion  The network data version.
     */
    void SetNetworkDataVersion(uint8_t aVersion) { mNetworkDataVersion = aVersion; }

    /**
     * Generates a new challenge value to use during a child attach.
     */
    void GenerateChallenge(void) { mAttachChallenge.GenerateRandom(); }

    /**
     * Gets the current challenge value used during attach.
     *
     * @returns The current challenge value.
     */
    const Mle::TxChallenge &GetChallenge(void) const { return mAttachChallenge; }

    /**
     * Clears the requested TLV list.
     */
    void ClearRequestTlvs(void) { memset(mRequestTlvs, Mle::Tlv::kInvalid, sizeof(mRequestTlvs)); }

    /**
     * Returns the requested TLV at index @p aIndex.
     *
     * @param[in]  aIndex  The index into the requested TLV list.
     *
     * @returns The requested TLV at index @p aIndex.
     */
    uint8_t GetRequestTlv(uint8_t aIndex) const { return mRequestTlvs[aIndex]; }

    /**
     * Sets the requested TLV at index @p aIndex.
     *
     * @param[in]  aIndex  The index into the requested TLV list.
     * @param[in]  aType   The TLV type.
     */
    void SetRequestTlv(uint8_t aIndex, uint8_t aType) { mRequestTlvs[aIndex] = aType; }

private:
    uint8_t mNetworkDataVersion;

    union
    {
        uint8_t          mRequestTlvs[kMaxRequestTlvs];
        Mle::TxChallenge mAttachChallenge;
    };
};

#endif // OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE

} // namespace ot

#endif // PEER_HPP_
