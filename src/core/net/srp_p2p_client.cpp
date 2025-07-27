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

#include "srp_p2p_client.hpp"

#if OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE && OPENTHREAD_CONFIG_P2P_ENABLE

#include "instance/instance.hpp"

/**
 * @file
 *   This file implements the SRP P2P client.
 */

namespace ot {
namespace Srp {

RegisterLogModule("SrpP2pClient");

bool P2pClient::NeighborInfo::SetSessionState(Client::State aState)
{
    bool changed = false;

    VerifyOrExit(mSessionState != aState);
    LogInfo("State %s -> %s", Client::StateToString(mSessionState), Client::StateToString(aState));

    mSessionState = aState;
    changed       = true;

exit:
    return changed;
}

P2pClient::P2pClient(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mSocket(aInstance, *this)
    , mTimer(aInstance)
{
}

Error P2pClient::PrepareSocket(void)
{
    Error error = kErrorNone;

    VerifyOrExit(!mSocket.IsOpen());
    SuccessOrExit(error = mSocket.Open(Ip6::kNetifThreadInternal));

    error = mSocket.Bind();
    if (error != kErrorNone)
    {
        LogInfo("Failed to open the socket: error: %s", ErrorToString(error));
        IgnoreError(mSocket.Close());
    }

exit:
    return error;
}

void P2pClient::HandleP2pEvent(Mle::P2pEvent aEvent, Peer &aPeer)
{
    if (aEvent == Mle::kP2pEventLinked)
    {
        HandleP2pEstablished(aPeer);
    }
    else if (aEvent == Mle::kP2pEventUnlinked)
    {
        HandleP2pTearDown(aPeer);
    }
}

void P2pClient::HandleP2pEstablished(Peer &aPeer)
{
    LogInfo("HandleP2pEstablished() --------------------------------------------------");

    SuccessOrExit(PrepareSocket());

    aPeer.SetSessionId(GetPeerIndex(aPeer));
    aPeer.ResetTxFailureRetryCount();
    aPeer.ResetRetryWaitInterval();

    SetSessionState(aPeer, Client::kStateToUpdate);
    UpdateState();

exit:
    return;
}

void P2pClient::HandleP2pTearDown(Peer &aPeer)
{
    // Change the state of host info and services so that they are
    // added/removed again once the client is started back. In the
    // case of `kAdding`, we intentionally move to `kToRefresh`
    // instead of `kToAdd` since the server may receive our add
    // request and the item may be registered on the server. This
    // ensures that if we are later asked to remove the item, we do
    // notify server.

    static const Client::ItemState kNewStateOnStop[]{
        /* (0) kToAdd      -> */ Client::kToAdd,
        /* (1) kAdding     -> */ Client::kToRefresh,
        /* (2) kToRefresh  -> */ Client::kToRefresh,
        /* (3) kRefreshing -> */ Client::kToRefresh,
        /* (4) kToRemove   -> */ Client::kToRemove,
        /* (5) kRemoving   -> */ Client::kToRemove,
        /* (6) kRegistered -> */ Client::kToRefresh,
        /* (7) kRemoved    -> */ Client::kRemoved,
    };

    VerifyOrExit(aPeer.GetSessionState() != Client::kStateStopped);

    // State changes:
    //   kAdding     -> kToRefresh
    //   kRefreshing -> kToRefresh
    //   kRemoving   -> kToRemove
    //   kRegistered -> kToRefresh

    Get<Client>().ChangeHostAndServiceStates(kNewStateOnStop, Client::kForAllServices, aPeer);

    SetSessionState(aPeer, Client::kStateStopped);

exit:
    return;
}

void P2pClient::HandleSetHostName(void)
{
    Get<Client>().mHostInfo.ResetAllP2pStates(Client::kToAdd);
    UpdateState();
}

void P2pClient::HandleAddService(Client::Service &aService)
{
    aService.ResetAllP2pStates(Client::kToAdd);
    UpdateState();
}

void P2pClient::HandleRemoveService(Client::Service &aService)
{
    for (uint8_t i = 0; i < kMaxNumP2pLinks; i++)
    {
        Peer *peer = Get<PeerTable>().GetPeerAtIndex(i);

        if (peer->GetState() != Neighbor::kStateValid)
        {
            aService.SetState(Client::kRemoved, i);
        }
        else if (aService.GetState(i) != Client::kRemoving)
        {
            aService.SetState(Client::kToRemove, i);
        }
    }

    UpdateState();
}

void P2pClient::HandleClearService(Client::Service &aService)
{
    aService.ResetAllP2pStates(Client::kRemoved);
    UpdateState();
}

void P2pClient::HandleRemoveHostAndServices(bool aShouldRemoveKeyLease, bool aSendUnregToServer)
{
    for (uint8_t i = 0; i < kMaxNumP2pLinks; i++)
    {
        Peer *peer = Get<PeerTable>().GetPeerAtIndex(i);

        if (peer->GetState() != Neighbor::kStateValid)
        {
            for (Client::Service &service : Get<Client>().mServices)
            {
                service.SetState(Client::kRemoved, i);
            }

            Get<Client>().mHostInfo.SetState(Client::kRemoved, i);
        }
        else
        {
            if (Get<Client>().UpdateStateForRemoveHostAndServices(*peer, aShouldRemoveKeyLease, aSendUnregToServer) !=
                kErrorNone)
            {
                continue;
            }

            if (Get<Client>().GetHostInfo().GetState(peer->GetSessionId()) == Client::kRemoved)
            {
                peer->ResetRetryWaitInterval();
                SetSessionState(*peer, Client::kStateUpdated);
            }
        }
    }

    UpdateState();
}

void P2pClient::HandleClearHostAndServices(void)
{
    for (Peer &peer : Get<PeerTable>().Iterate(Peer::kInStateValid))
    {
        switch (peer.GetSessionState())
        {
        case Client::kStateStopped:
        case Client::kStatePaused:
            break;

        case Client::kStateToUpdate:
        case Client::kStateUpdating:
        case Client::kStateUpdated:
        case Client::kStateToRetry:
            SetSessionState(peer, Client::kStateUpdated);
            break;
        }

        peer.ResetTxFailureRetryCount();
        peer.ResetRetryWaitInterval();
    }
}

void P2pClient::UpdateState(void)
{
    Error error;

    for (Peer &peer : Get<PeerTable>().Iterate(Peer::kInStateValid))
    {
        NextFireTime nextRenewTime;
        bool         shouldUpdate;

        if (peer.GetSessionState() == Client::kStateStopped || peer.GetSessionState() == Client::kStatePaused)
        {
            continue;
        }

        error = Get<Client>().UpdateHostAndServiceState(peer, shouldUpdate, nextRenewTime);
        if (error != kErrorNone)
        {
            continue;
        }

        if (shouldUpdate)
        {
            SetSessionState(peer, Client::kStateToUpdate);
            continue;
        }

        if (peer.GetSessionState() == Client::kStateUpdated)
        {
            TimerStartAt(peer, nextRenewTime);
        }
    }
}

void P2pClient::SetSessionState(Peer &aPeer, Client::State aState)
{
    VerifyOrExit(aPeer.SetSessionState(aState));

    switch (aPeer.GetSessionState())
    {
    case Client::kStateStopped:
    case Client::kStatePaused:
    case Client::kStateUpdated:
        TimerStop(aPeer);
        break;

    case Client::kStateToUpdate:
        TimerStart(aPeer, Random::NonCrypto::GetUint32InRange(kMinTxJitter, kMaxTxJitter));
        break;

    case Client::kStateUpdating:
        TimerStart(aPeer, aPeer.GetRetryWaitInterval());
        break;

    case Client::kStateToRetry:
        break;
    }

exit:
    return;
}

void P2pClient::UpdateTimer(void)
{
    NextFireTime nextExpireTime;
    bool         shouldFireNow = false;

    mTimer.Stop();
    LogInfo("UpdateTimer() -> mTimer.Stop()");

    for (Peer &peer : Get<PeerTable>().Iterate(Peer::kInStateValid))
    {
        if (!peer.IsTimerRunning())
        {
            continue;
        }

        if (peer.GetTimerFireTime() <= nextExpireTime.GetNow())
        {
            shouldFireNow = true;
            break;
        }
        else
        {
            nextExpireTime.UpdateIfEarlier(peer.GetTimerFireTime());
        }
    }

    if (shouldFireNow)
    {
        LogInfo("UpdateTimer() -> mTimer.Start(0)");
        mTimer.Start(0);
        ExitNow();
    }

    LogInfo("UpdateTimer() -> mTimer.FireAt(%u)",
            nextExpireTime.GetNextTime().GetValue() - nextExpireTime.GetNow().GetValue());

    mTimer.FireAt(nextExpireTime);

exit:
    return;
}

void P2pClient::HandleTimer(void)
{
    NextFireTime nextExpireTime;

    LogInfo("HandleTimer()");

    for (Peer &peer : Get<PeerTable>().Iterate(Peer::kInStateValid))
    {
        if (!peer.IsTimerRunning())
        {
            continue;
        }

        if (peer.GetTimerFireTime() > nextExpireTime.GetNow())
        {
            continue;
        }

        peer.SetTimerRunning(false);

        LogInfo("HandleTimer() [%u] State=%s", peer.GetSessionId(), Client::StateToString(peer.GetSessionState()));

        switch (peer.GetSessionState())
        {
        case Client::kStateStopped:
        case Client::kStatePaused:
            break;

        case Client::kStateToUpdate:
        case Client::kStateToRetry:
            SendUpdate(peer);
            break;

        case Client::kStateUpdating:
            peer.LogRetryWaitInterval();
            LogInfo("Timed out, no response");
            peer.GrowRetryWaitInterval();
            SetSessionState(peer, Client::kStateToUpdate);
            // InvokeCallback(kErrorResponseTimeout);

            break;

        case Client::kStateUpdated:
            UpdateState();
            break;
        }
    }

    UpdateTimer();
}

void P2pClient::SendUpdate(Peer &aPeer)
{
    Error            error = kErrorNone;
    Ip6::Address     peerAddress;
    Ip6::MessageInfo msgInfo;
    Client::Update   update(GetInstance());

    LogInfo("SendUpdate() --------------------------------------->");
    SuccessOrExit(error = update.SetMessage(mSocket.NewMessage()));
    SuccessOrExit(error = Get<Client>().GenerateUpdateMessage(update, aPeer));

    aPeer.GetLinkLocalIp6Address(peerAddress);
    msgInfo.SetPeerAddr(peerAddress);
    msgInfo.mPeerPort = NeighborInfo::kP2pModeSrpServerPort;

    SuccessOrExit(error = mSocket.SendTo(update.GetMessage(), msgInfo));

    // Ownership of the message is transferred to the socket upon a
    // successful `SendTo()` call.
    update.ReleaseMessage();

    LogInfo("Send update, msg-id:0x%x", aPeer.GetMessageId());

    aPeer.SetLeaseRenewTime(TimerMilli::GetNow());
    aPeer.ResetTxFailureRetryCount();
    SetSessionState(aPeer, Client::kStateUpdating);

exit:
    if (error != kErrorNone)
    {
        // If there is an error in preparation or transmission of the
        // update message (e.g., no buffer to allocate message), up to
        // `kMaxTxFailureRetries` times, we wait for a short interval
        // `kTxFailureRetryInterval` and try again. After this, we
        // continue to retry using the `mRetryWaitInterval` (which keeps
        // growing on each failure).

        LogInfo("Failed to send update: %s", ErrorToString(error));

        SetSessionState(aPeer, Client::kStateToRetry);

        if (aPeer.GetTxFailureRetryCount() < Client::kMaxTxFailureRetries)
        {
            uint32_t interval;

            aPeer.IncrementTxFailureRetryCount();
            interval = Random::NonCrypto::AddJitter(Client::kTxFailureRetryInterval, Client::kTxFailureRetryJitter);
            TimerStart(aPeer, interval);

            LogInfo("Quick retry %u in %lu msec", aPeer.GetTxFailureRetryCount(), ToUlong(interval));

            // Do not report message preparation errors to user
            // until `kMaxTxFailureRetries` are exhausted.
        }
        else
        {
            aPeer.LogRetryWaitInterval();
            TimerStart(aPeer, Random::NonCrypto::AddJitter(aPeer.GetRetryWaitInterval(), Client::kRetryIntervalJitter));
            aPeer.GrowRetryWaitInterval();
            // InvokeCallback(error);
        }
    }

    return;
}

void P2pClient::HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    Mac::ExtAddress extAddress;
    Peer           *peer;

    VerifyOrExit(aMessageInfo.GetPeerAddr().IsLinkLocalUnicast());
    extAddress.SetFromIid(aMessageInfo.GetPeerAddr().GetIid());

    peer = Get<PeerTable>().FindPeer(extAddress, Peer::kInStateValid);
    VerifyOrExit(peer != nullptr);

    ProcessResponse(*peer, aMessage);

exit:
    return;
}

void P2pClient::ProcessResponse(Peer &aPeer, Message &aMessage)
{
    Error                       error = kErrorNone;
    Dns::UpdateHeader           header;
    LinkedList<Client::Service> removedServices;
    Client::Response            response(aMessage);

    LogInfo("ProcessResponse() --------------------------------------->");

    VerifyOrExit(Get<Client>().IsValidStateToProcessResponse(aPeer.GetSessionState()));
    SuccessOrExit(error = response.ReadUpdateHeader(header));
    VerifyOrExit(header.GetMessageId() == aPeer.GetMessageId(), error = kErrorDrop);

    LogInfo("Received response, msg-id:0x%x", header.GetMessageId());

    error = Dns::Header::ResponseCodeToError(header.GetResponseCode());

    if (error != kErrorNone)
    {
        LogInfo("Server rejected %s code:%d", ErrorToString(error), header.GetResponseCode());

        if (Get<Client>().mHostInfo.GetState(GetPeerIndex(aPeer)) == Client::kAdding)
        {
            // Since server rejected the update message, we go back to
            // `Client::kToAdd` state to allow user to give a new name using
            // `SetHostName()`.
            Get<Client>().mHostInfo.SetState(Client::kToAdd, GetPeerIndex(aPeer));
        }

        // Wait for the timer to expire to retry. Note that timer is
        // already scheduled for the current wait interval when state
        // was changed to `Client::kStateUpdating`.

        aPeer.LogRetryWaitInterval();
        aPeer.GrowRetryWaitInterval();
        SetSessionState(aPeer, Client::kStateToRetry);
        // InvokeCallback(error);

        ExitNow(error = kErrorNone);
    }

    SuccessOrExit(error = response.ProcessRecords(header, aPeer.mLease, aPeer.mKeyLease));
    Get<Client>().UpdateStateAndLeaseRenewTime(aPeer);

    aPeer.ResetRetryWaitInterval();
    SetSessionState(aPeer, Client::kStateUpdated);

    if ((Get<Client>().mHostInfo.AllSessionInState(Client::kRemoved)) || (AnyServiceRemoved()))
    {
        Get<Client>().HandleUpdateDone();
    }

    UpdateState();

exit:
    if (error != kErrorNone)
    {
        LogInfo("Failed to process response %s", ErrorToString(error));
    }
}

bool P2pClient::AnyServiceRemoved(void)
{
    bool ret = false;

    for (Client::Service &service : Get<Client>().mServices)
    {
        if (service.AllSessionInState(Client::kRemoved))
        {
            ExitNow(ret = true);
        }
    }

exit:
    return ret;
}

void P2pClient::TimerStart(Peer &aPeer, uint32_t aDelay)
{
    TimeMilli now = TimerMilli::GetNow();

    aPeer.SetTimerRunning(true);
    aPeer.SetTimerFireTime(now + aDelay);

    UpdateTimer();
}

void P2pClient::TimerStartAt(Peer &aPeer, NextFireTime aNextFireTime)
{
    TimeMilli delay(0);

    VerifyOrExit(aNextFireTime.IsSet());

    aPeer.SetTimerRunning(true);
    aPeer.SetTimerFireTime(aNextFireTime.GetNextTime());

    UpdateTimer();

exit:
    return;
}

void P2pClient::TimerStop(Peer &aPeer)
{
    aPeer.SetTimerRunning(false);
    UpdateTimer();
}

uint8_t P2pClient::GetPeerIndex(const Peer &aPeer) const { return Get<PeerTable>().GetPeerIndex(aPeer); }
} // namespace Srp
} // namespace ot

#endif // OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE && OPENTHREAD_CONFIG_P2P_ENABLE
