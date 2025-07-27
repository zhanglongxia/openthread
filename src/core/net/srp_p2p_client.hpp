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

#ifndef SRP_P2P_CLIENT_HPP_
#define SRP_P2P_CLIENT_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE && OPENTHREAD_CONFIG_P2P_ENABLE
#include <openthread/srp_client.h>

#include "common/as_core_type.hpp"
#include "common/callback.hpp"
#include "common/clearable.hpp"
#include "common/heap_allocatable.hpp"
#include "common/linked_list.hpp"
#include "common/locator.hpp"
#include "common/log.hpp"
#include "common/message.hpp"
#include "common/non_copyable.hpp"
#include "common/notifier.hpp"
#include "common/numeric_limits.hpp"
#include "common/owned_ptr.hpp"
#include "common/random.hpp"
#include "common/timer.hpp"
#include "crypto/ecdsa.hpp"
#include "net/dns_types.hpp"
#include "net/ip6.hpp"
#include "net/netif.hpp"
#include "net/srp_client.hpp"
#include "net/udp6.hpp"
#include "thread/network_data_service.hpp"

/**
 * @file
 *   This file includes definitions for the SRP (Service Registration Protocol) P2P client.
 */

namespace ot {

class Peer;

namespace Srp {

/**
 * Implements SRP P2P client.
 */
class P2pClient : public InstanceLocator, private NonCopyable
{
    friend class ot::Notifier;
    friend class ot::Ip6::Netif;
    friend class Client::Service;

public:
    static constexpr uint8_t kMaxNumP2pLinks = OPENTHREAD_CONFIG_P2P_MAX_PEERS;

    explicit P2pClient(Instance &aInstance);

    void HandleSetHostName(void);
    void HandleAddService(Client::Service &aService);
    void HandleRemoveService(Client::Service &aService);
    void HandleClearService(Client::Service &aService);
    void HandleRemoveHostAndServices(bool aShouldRemoveKeyLease, bool aSendUnregToServer);
    void HandleClearHostAndServices(void);

    /**
     * Receives the P2P link state update result.
     *
     * @param[in]  aEvent The P2P link event.
     * @param[in]  aPeer  A reference to the peer instance.
     */
    void HandleP2pEvent(Mle::P2pEvent aEvent, Peer &aPeer);

    class NeighborInfo : public Client::Session
    {
        friend P2pClient;

    public:
        static constexpr uint16_t kP2pModeSrpServerPort = 53;

        bool SetSessionState(Client::State aState);

    private:
        bool      IsTimerRunning(void) const { return mTimerIsRunning; }
        void      SetTimerRunning(bool aRunning) { mTimerIsRunning = aRunning; }
        TimeMilli GetTimerFireTime(void) const { return mTimerFireTime; }
        void      SetTimerFireTime(TimeMilli aTime) { mTimerFireTime = aTime; }

        bool      mTimerIsRunning : 1;
        TimeMilli mTimerFireTime;
    };

private:
    static constexpr uint32_t kMinTxJitter = 5;
    static constexpr uint32_t kMaxTxJitter = 50;

    Error   PrepareSocket(void);
    void    SendUpdate(Peer &aPeer);
    void    UpdateTimer(void);
    void    HandleTimer(void);
    void    UpdateState(void);
    void    HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    void    ProcessResponse(Peer &aPeer, Message &aMessage);
    void    HandleP2pEstablished(Peer &aPeer);
    void    HandleP2pTearDown(Peer &aPeer);
    void    TimerStart(Peer &aPeer, uint32_t aDelay);
    void    TimerStartAt(Peer &aPeer, NextFireTime aNextFireTime);
    void    TimerStop(Peer &aPeer);
    void    SetSessionState(Peer &aPeer, Client::State aState);
    uint8_t GetPeerIndex(const Peer &aPeer) const;
    bool    AnyServiceRemoved(void);

    using DelayTimer   = TimerMilliIn<P2pClient, &P2pClient::HandleTimer>;
    using ClientSocket = Ip6::Udp::SocketIn<P2pClient, &P2pClient::HandleUdpReceive>;

    ClientSocket mSocket;
    DelayTimer   mTimer;
};

} // namespace Srp
} // namespace ot

#endif // OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE && OPENTHREAD_CONFIG_P2P_ENABLE

#endif // SRP_P2P_CLIENT_HPP_
