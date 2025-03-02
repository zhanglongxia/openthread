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
 *   This file implements MLE functionality required for the peer to peer link.
 */

#include "mle.hpp"

#if OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE

#include <openthread/platform/toolchain.h>

#include "instance/instance.hpp"
#include "utils/static_counter.hpp"

namespace ot {
namespace Mle {

RegisterLogModule("MlePeer");

#if OPENTHREAD_CONFIG_WAKEUP_COORDINATOR_ENABLE
Error Mle::P2pWakeupAndConnect(const Mac::WakeupAddress &aWakeupAddress,
                               uint16_t                  aIntervalUs,
                               uint16_t                  aDurationMs,
                               otP2pConnectedCallback    aCallback,
                               void                     *aContext)
{
    Error error;

    VerifyOrExit(aWakeupAddress.IsValid(), error = kErrorInvalidArgs);
    VerifyOrExit((aIntervalUs > 0) && (aDurationMs > 0), error = kErrorInvalidArgs);
    VerifyOrExit(aIntervalUs < aDurationMs * Time::kOneMsecInUsec, error = kErrorInvalidArgs);
    VerifyOrExit(mP2pState == kP2pIdle, error = kErrorInvalidState);
    SuccessOrExit(error = mWakeupTxScheduler.WakeUp(aWakeupAddress, aIntervalUs, aDurationMs));

    mP2pState = kP2pAttaching;
    mP2pConnectedCallback.Set(aCallback, aContext);
    Get<MeshForwarder>().SetRxOnWhenIdle(true);
    mP2pTimer.FireAt(mWakeupTxScheduler.GetTxEndTime() + mWakeupTxScheduler.GetConnectionWindowUs());

    LogInfo("Start to connect to %s", aWakeupAddress.ToString().AsCString());

exit:
    return error;
}
#endif // OPENTHREAD_CONFIG_WAKEUP_COORDINATOR_ENABLE

void Mle::HandleP2pTimer(void)
{
    switch (mP2pState)
    {
    case kP2pAttaching:
        // Connection timeout
        if (!IsRxOnWhenIdle())
        {
            Get<MeshForwarder>().SetRxOnWhenIdle(false);
        }

        LogInfo("Connection window closed");

        mP2pState = kP2pIdle;
        mP2pConnectedCallback.InvokeAndClearIfSet(kErrorFailed);
        break;

    case kP2pDetaching:
    {
        OT_ASSERT(mP2pPeer != nullptr);

#if OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE
        Get<Srp::Client>().P2pSrpClientStop(mP2pPeer->GetExtAddress());
#endif
        mP2pEventCallback.InvokeIfSet(OT_P2P_EVENT_DISCONNECTED, &mP2pPeer->GetExtAddress());
        // Triger the ChildSupervisor to not send supervision messages to keep the link alive.
        Get<NeighborTable>().Signal(NeighborTable::kChildRemoved, *mP2pPeer);
        mP2pPeer->SetState(Neighbor::kStateLinkRequest);
        mP2pState = kP2pIdle;
    }
    break;

    default:
        break;
    }
}

void Mle::P2pSetEventCallback(otP2pEventCallback aCallback, void *aContext)
{
    mP2pEventCallback.Set(aCallback, aContext);
}

#if OPENTHREAD_CONFIG_SRP_SERVER_ENABLE
void Mle::HandleServerStateChange(void) { SrpServerUpdate(); }

void Mle::SrpServerUpdate(void)
{
    uint16_t srpServerPort = Get<Srp::Server>().GetPort();

    switch (Get<Srp::Server>().GetState())
    {
    case Srp::Server::kStateDisabled:
        LogInfo("Srp::Server::kStateDisabled ---------------------------->");
        LinkDataUpdate(false, srpServerPort);
        break;

    case Srp::Server::kStateStopped:
        LogInfo("Srp::Server::kStateStopped ---------------------------->");
        LinkDataUpdate(false, srpServerPort);
        break;

    case Srp::Server::kStateRunning:
        LogInfo("Srp::Server::kStateRunning ---------------------------->");
        LinkDataUpdate(true, srpServerPort);
        break;
    }
}

void Mle::LinkDataUpdate(bool aSrpServerEnabled, uint16_t aSrpServerPort)
{
    for (Child &child : Get<ChildTable>().Iterate(Child::kInStateValid))
    {
        if (!child.IsP2pPeer())
        {
            continue;
        }

        SendLinkDataUpdate(child, aSrpServerEnabled, aSrpServerPort);
    }
}

void Mle::SendLinkDataUpdate(Child &aChild, bool aIsLocalSrpServer, uint16_t aSrpServerPort)
{
    Error        error = kErrorNone;
    TxMessage   *message;
    Ip6::Address destination;

    destination.Clear();
    destination.SetToLinkLocalAddress(aChild.GetExtAddress());

    LogInfo("SendLinkDataUpdate ---------------------------->");
    VerifyOrExit((message = NewMleMessage(kCommandLinkDataUpdate)) != nullptr, error = kErrorNoBufs);
    SuccessOrExit(error = message->AppendLinkDataTlv(aIsLocalSrpServer, aSrpServerPort));
    SuccessOrExit(error = message->SendTo(destination));

exit:
    return;
}
#endif

void Mle::HandleLinkDataUpdate(RxInfo &aRxInfo)
{
    Child   *peer;
    bool     isLocalSrpServer;
    uint16_t srpServerPort;

    LogInfo("HandleLinkDataUpdate ------------------------->");
    VerifyOrExit(aRxInfo.mMessageInfo.GetPeerAddr().IsLinkLocalUnicast());

    ProcessKeySequence(aRxInfo);

    VerifyOrExit(aRxInfo.mNeighbor != nullptr);
    SuccessOrExit(aRxInfo.mMessage.ReadLinkDataTlv(isLocalSrpServer, srpServerPort));

    peer = static_cast<Child *>(aRxInfo.mNeighbor);
    peer->SetLocalSrpServer(isLocalSrpServer);

#if OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE
    if (isLocalSrpServer)
    {
        LogInfo("Srp client to %s is started ------------------------------>",
                peer->GetExtAddress().ToString().AsCString());
        Get<Srp::Client>().P2pSrpClientStart(peer->GetExtAddress(), srpServerPort);
    }
#endif

exit:
    return;
}

Error Mle::P2pDisconnect(const Mac::ExtAddress &aExtAddress)
{
    Error        error = kErrorNone;
    TxMessage   *message;
    Ip6::Address destination;

    VerifyOrExit(mP2pState == kP2pIdle, error = kErrorBusy);
    mP2pPeer = Get<ChildTable>().FindChild(aExtAddress, Child::kInStateAnyExceptInvalid);
    VerifyOrExit(mP2pPeer != nullptr, error = kErrorNotFound);

    destination.Clear();
    destination.SetToLinkLocalAddress(aExtAddress);

    LogInfo("SendP2pLinkTearDown ---------------------------->");
    VerifyOrExit((message = NewMleMessage(kCommandLinkTearDown)) != nullptr, error = kErrorNoBufs);
    SuccessOrExit(error = message->SendTo(destination));
    mP2pState = kP2pDetaching;
    mP2pTimer.Start(kMaxP2pKeepAliveBeforeRemovePeer);

exit:
    return error;
}

void Mle::HandleLinkTearDown(RxInfo &aRxInfo)
{
    VerifyOrExit(aRxInfo.mMessageInfo.GetPeerAddr().IsLinkLocalUnicast());

    ProcessKeySequence(aRxInfo);

    VerifyOrExit(aRxInfo.mNeighbor != nullptr);

    mP2pPeer  = static_cast<Child *>(aRxInfo.mNeighbor);
    mP2pState = kP2pDetaching;
    mP2pTimer.Start(kMaxP2pKeepAliveBeforeRemovePeer);

exit:
    return;
}

void Mle::SendP2pLinkRequest(const Mac::ExtAddress &aExtAddress)
{
    Error        error   = kErrorNone;
    TxMessage   *message = nullptr;
    Child       *peer;
    Ip6::Address destination;

    LogInfo("SendP2pLinkRequest ----------------------------->");
    VerifyOrExit((message = NewMleMessage(kCommandLinkRequest)) != nullptr, error = kErrorNoBufs);
    SuccessOrExit(error = message->AppendModeTlv(GetDeviceMode()));
    SuccessOrExit(error = message->AppendVersionTlv());

    peer = Get<ChildTable>().FindChild(aExtAddress, Child::kInStateAnyExceptInvalid);
    if (peer != nullptr)
    {
        VerifyOrExit(peer->GetState() != Child::kStateLinkRequest);
    }
    else
    {
        VerifyOrExit((peer = Get<ChildTable>().GetNewChild(Child::kNeighborTypePeer)) != nullptr, error = kErrorNoBufs);
    }

    peer->GenerateChallenge();
    SuccessOrExit(error = message->AppendChallengeTlv(peer->GetChallenge()));

    destination.Clear();
    destination.SetToLinkLocalAddress(aExtAddress);

    // Keep the radio in rx state for receiving LinkAcceptAndRequest.
    Get<MeshForwarder>().SetRxOnWhenIdle(true);

    SuccessOrExit(error = message->SendTo(destination));

    peer->GetLinkInfo().Clear();
    peer->ResetLinkFailures();
    peer->SetLastHeard(TimerMilli::GetNow());
    peer->SetExtAddress(aExtAddress);
    peer->RestartLinkAcceptTimeout();
    peer->SetState(Neighbor::kStateLinkRequest);

    Log(kMessageSend, kTypeLinkRequest, destination);

exit:
    FreeMessageOnError(message, error);
}

void Mle::HandleP2pLinkRequest(RxInfo &aRxInfo)
{
    Error          error = kErrorNone;
    LinkAcceptInfo info;
    DeviceMode     mode;
    uint16_t       version;

    LogInfo("HandleP2pLinkRequest --------------------------->");
    Log(kMessageReceive, kTypeLinkRequest, aRxInfo.mMessageInfo.GetPeerAddr());
    VerifyOrExit(aRxInfo.mMessageInfo.GetPeerAddr().IsLinkLocalUnicast());

    VerifyOrExit(mP2pTimer.IsRunning());
    mP2pTimer.Stop();

    SuccessOrExit(error = aRxInfo.mMessage.ReadModeTlv(mode));
    SuccessOrExit(error = aRxInfo.mMessage.ReadChallengeTlv(info.mRxChallenge));
    SuccessOrExit(error = aRxInfo.mMessage.ReadVersionTlv(version));

    aRxInfo.mMessageInfo.GetPeerAddr().GetIid().ConvertToExtAddress(info.mExtAddress);
    ProcessKeySequence(aRxInfo);

    if (aRxInfo.mNeighbor == nullptr)
    {
        VerifyOrExit((aRxInfo.mNeighbor = Get<ChildTable>().GetNewChild(Child::kNeighborTypePeer)) != nullptr,
                     error = kErrorNoBufs);
    }

    InitNeighbor(*aRxInfo.mNeighbor, aRxInfo);
    aRxInfo.mNeighbor->SetDeviceMode(mode);
    aRxInfo.mNeighbor->SetVersion(version);
    aRxInfo.mNeighbor->SetState(Neighbor::kStateLinkRequest);

    info.mLinkMargin = Get<Mac::Mac>().ComputeLinkMargin(aRxInfo.mMessage.GetAverageRss());

    SuccessOrExit(error = SendP2pLinkAcceptAndRequest(info));
#if OPENTHREAD_CONFIG_WAKEUP_COORDINATOR_ENABLE
    mWakeupTxScheduler.Stop();
#endif

exit:
    LogProcessError(kTypeLinkRequest, error);
}

Error Mle::SendP2pLinkAccept(const LinkAcceptInfo &aInfo)
{
    return SendP2pLinkAcceptVariant(aInfo, false /* aIsLinkAcceptAndRequest*/);
}

Error Mle::SendP2pLinkAcceptAndRequest(const LinkAcceptInfo &aInfo)
{
    return SendP2pLinkAcceptVariant(aInfo, true /* aIsLinkAcceptAndRequest*/);
}

Error Mle::SendP2pLinkAcceptVariant(const LinkAcceptInfo &aInfo, bool aIsLinkAcceptAndRequest)
{
    Error        error   = kErrorNone;
    TxMessage   *message = nullptr;
    Command      command = aIsLinkAcceptAndRequest ? kCommandLinkAcceptAndRequest : kCommandLinkAccept;
    Child       *peer    = nullptr;
    Ip6::Address destination;

    if (aIsLinkAcceptAndRequest)
    {
        LogInfo("SendP2pLinkAcceptAndRequest -------------------->");
    }
    else
    {
        LogInfo("SendP2pLinkAccept ------------------------------>");
    }

    VerifyOrExit((message = NewMleMessage(command)) != nullptr, error = kErrorNoBufs);
    if (command == kCommandLinkAcceptAndRequest)
    {
        SuccessOrExit(error = message->AppendModeTlv(GetDeviceMode()));
        SuccessOrExit(error = message->AppendVersionTlv());
    }

    SuccessOrExit(error = message->AppendResponseTlv(aInfo.mRxChallenge));
    if (command == kCommandLinkAcceptAndRequest)
    {
        VerifyOrExit((peer = Get<ChildTable>().FindChild(aInfo.mExtAddress, Child::kInStateLinkRequest)) != nullptr,
                     error = kErrorNotFound);
        peer->GenerateChallenge();
        SuccessOrExit(error = message->AppendChallengeTlv(peer->GetChallenge()));
        if (peer->IsRxOnWhenIdle())
        {
            message->SetDirectTransmission();
        }
        else
        {
            message->ClearDirectTransmission();
        }
    }

    SuccessOrExit(error = message->AppendLinkMarginTlv(aInfo.mLinkMargin));
    SuccessOrExit(error = message->AppendLinkAndMleFrameCounterTlvs());
    SuccessOrExit(error = message->AppendSupervisionIntervalTlvIfSleepyChild());
    SuccessOrExit(error = message->AppendCslClockAccuracyTlv());

    destination.SetToLinkLocalAddress(aInfo.mExtAddress);

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (!IsRxOnWhenIdle())
    {
        Get<Mac::Mac>().SetCslCapable(true /* aIsCslCapable*/);
    }
#endif

    SuccessOrExit(error = message->SendTo(destination));

    if (command == kCommandLinkAcceptAndRequest)
    {
        // Triger the ChildSupervisor to send supervision messages to keep the link alive.
        Get<NeighborTable>().Signal(NeighborTable::kChildAdded, *peer);
        mP2pState = kP2pIdle;
        LogInfo("P2P link to %s is established ------------------------------>",
                aInfo.mExtAddress.ToString().AsCString());
        mP2pEventCallback.InvokeIfSet(OT_P2P_EVENT_CONNECTED, &aInfo.mExtAddress);
    }

    Log(kMessageSend, (command == kCommandLinkAccept) ? kTypeLinkAccept : kTypeLinkAcceptAndRequest, destination);

exit:
    FreeMessageOnError(message, error);
    return error;
}

void Mle::HandleP2pLinkAccept(RxInfo &aRxInfo)
{
    LogInfo("HandleP2pLinkAccept ---------------------------->");
    HandleP2pLinkAcceptVariant(aRxInfo, kTypeLinkAccept);
}

void Mle::HandleP2pLinkAcceptAndRequest(RxInfo &aRxInfo)
{
    LogInfo("HandleP2pLinkAcceptAndRequest ------------------>");
    HandleP2pLinkAcceptVariant(aRxInfo, kTypeLinkAcceptAndRequest);
}

void Mle::HandleP2pLinkAcceptVariant(RxInfo &aRxInfo, MessageType aMessageType)
{
    // Handles "Link Accept" or "Link Accept And Request".

    Error          error = kErrorNone;
    DeviceMode     mode;
    uint16_t       version;
    RxChallenge    response;
    uint32_t       linkFrameCounter;
    uint32_t       mleFrameCounter;
    Child         *peer;
    LinkAcceptInfo info;
    uint8_t        linkMargin;
    uint16_t       supervisionInterval;

    Log(kMessageReceive, aMessageType, aRxInfo.mMessageInfo.GetPeerAddr());

    if (aRxInfo.mNeighbor == nullptr)
    {
        aRxInfo.mMessageInfo.GetPeerAddr().GetIid().ConvertToExtAddress(info.mExtAddress);
        VerifyOrExit((peer = Get<ChildTable>().FindChild(info.mExtAddress, Child::kInStateLinkRequest)) != nullptr);
        aRxInfo.mNeighbor = peer;
    }
    else
    {
        peer = static_cast<Child *>(aRxInfo.mNeighbor);
    }

    if (aMessageType == kTypeLinkAcceptAndRequest)
    {
        SuccessOrExit(error = aRxInfo.mMessage.ReadModeTlv(mode));
        SuccessOrExit(error = aRxInfo.mMessage.ReadVersionTlv(version));
        peer->SetDeviceMode(mode);
        peer->SetVersion(version);
    }

    SuccessOrExit(error = aRxInfo.mMessage.ReadResponseTlv(response));
    VerifyOrExit(response == peer->GetChallenge());
    SuccessOrExit(error = aRxInfo.mMessage.ReadFrameCounterTlvs(linkFrameCounter, mleFrameCounter));
    SuccessOrExit(error = Tlv::Find<LinkMarginTlv>(aRxInfo.mMessage, linkMargin));

    switch (Tlv::Find<SupervisionIntervalTlv>(aRxInfo.mMessage, supervisionInterval))
    {
    case kErrorNone:
        break;
    case kErrorNotFound:
        supervisionInterval = 0;
        break;
    default:
        ExitNow(error = kErrorParse);
    }

    InitNeighbor(*peer, aRxInfo);

    peer->SetState(Neighbor::kStateValid);
    peer->GetLinkFrameCounters().SetAll(linkFrameCounter);
    peer->SetLinkAckFrameCounter(linkFrameCounter);
    peer->SetMleFrameCounter(mleFrameCounter);
    peer->SetState(Neighbor::kStateValid);
    peer->SetKeySequence(aRxInfo.mKeySequence);
    peer->SetSupervisionInterval(supervisionInterval);
    peer->ClearLinkAcceptTimeout();
    aRxInfo.mClass = RxInfo::kAuthoritativeMessage;

    ProcessKeySequence(aRxInfo);

    if (aMessageType == kTypeLinkAcceptAndRequest)
    {
        SuccessOrExit(error = aRxInfo.mMessage.ReadChallengeTlv(info.mRxChallenge));

        info.mExtAddress = aRxInfo.mNeighbor->GetExtAddress();
        info.mLinkMargin = Get<Mac::Mac>().ComputeLinkMargin(aRxInfo.mMessage.GetAverageRss());

        Get<MeshForwarder>().SetRxOnWhenIdle(IsRxOnWhenIdle());
        SuccessOrExit(error = SendP2pLinkAccept(info));
    }
    else
    {
        Get<MeshForwarder>().SetRxOnWhenIdle(IsRxOnWhenIdle());

        // Triger the ChildSupervisor to send supervision messages to keep the link alive.
        Get<NeighborTable>().Signal(NeighborTable::kChildAdded, *peer);
        mP2pState = kP2pIdle;

        LogInfo("P2P link to %s is established ------------------>", peer->GetExtAddress().ToString().AsCString());
        mP2pConnectedCallback.InvokeAndClearIfSet(kErrorNone);
        mP2pEventCallback.InvokeIfSet(OT_P2P_EVENT_CONNECTED, &peer->GetExtAddress());
    }

exit:
    LogProcessError(aMessageType, error);
}
} // namespace Mle
} // namespace ot
#endif // OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE
