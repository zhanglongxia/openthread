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

RegisterLogModule("MleChild");

#if OPENTHREAD_CONFIG_WAKEUP_COORDINATOR_ENABLE
Error Mle::P2pConnect(const Mac::WakeupAddress &aWakeupAddress,
                      uint16_t                  aIntervalUs,
                      uint16_t                  aDurationMs,
                      otP2pConnectedCallback    aCallback,
                      void                     *aContext)
{
    Error error;

    VerifyOrExit(aWakeupAddress.IsValid(), error = kErrorInvalidArgs);
    VerifyOrExit((aIntervalUs > 0) && (aDurationMs > 0), error = kErrorInvalidArgs);
    VerifyOrExit(aIntervalUs < aDurationMs * Time::kOneMsecInUsec, error = kErrorInvalidArgs);
    VerifyOrExit(mWedAttachState == kWedDetached, error = kErrorInvalidState);
    SuccessOrExit(error = mWakeupTxScheduler.WakeUp(aWakeupAddress, aIntervalUs, aDurationMs));

    mWedAttachState = kWedAttaching;
    mP2pConnectedCallback.Set(aCallback, aContext);
    Get<MeshForwarder>().SetRxOnWhenIdle(true);
    mWedAttachTimer.FireAt(mWakeupTxScheduler.GetTxEndTime() + mWakeupTxScheduler.GetConnectionWindowUs());

    LogInfo("Start to connect to %s", aWakeupAddress.ToString().AsCString());

exit:
    return error;
}

void Mle::HandleWedAttachTimer(void)
{
    switch (mWedAttachState)
    {
    case kWedAttaching:
        // Connection timeout
        if (!IsRxOnWhenIdle())
        {
            Get<MeshForwarder>().SetRxOnWhenIdle(false);
        }

        LogInfo("Connection window closed");

        mWedAttachState = kWedDetached;
        mP2pConnectedCallback.InvokeAndClearIfSet(kErrorFailed);
        break;
    default:
        break;
    }
}
#endif // OPENTHREAD_CONFIG_WAKEUP_COORDINATOR_ENABLE

void Mle::P2pSetEventCallback(otP2pEventCallback aCallback, void *aContext)
{
    mP2pEventCallback.Set(aCallback, aContext);
}

Error Mle::P2pDisconnect(const Mac::ExtAddress &) { return kErrorInvalidState; }

void Mle::SendP2pLinkRequest(const Mac::WakeupInfo &aWakeupInfo)
{
    Error        error   = kErrorNone;
    TxMessage   *message = nullptr;
    Child       *peer;
    Ip6::Address destination;

    LogInfo("SendP2pLinkRequest ---------------------> %u", kCommandLinkRequest);
    VerifyOrExit((message = NewMleMessage(kCommandLinkRequest)) != nullptr, error = kErrorNoBufs);
    SuccessOrExit(error = message->AppendModeTlv(GetDeviceMode()));
    SuccessOrExit(error = message->AppendVersionTlv());

    peer = Get<ChildTable>().FindChild(aWakeupInfo.mWcAddress, Child::kInStateAnyExceptInvalid);
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
    destination.SetToLinkLocalAddress(aWakeupInfo.mWcAddress);
    SuccessOrExit(error = message->SendTo(destination));

    peer->GetLinkInfo().Clear();
    peer->ResetLinkFailures();
    peer->SetLastHeard(TimerMilli::GetNow());
    peer->SetExtAddress(aWakeupInfo.mWcAddress);
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

    LogInfo("HandleP2pLinkRequest ------------------->");
    Log(kMessageReceive, kTypeLinkRequest, aRxInfo.mMessageInfo.GetPeerAddr());
    VerifyOrExit(aRxInfo.mMessageInfo.GetPeerAddr().IsLinkLocalUnicast());

    VerifyOrExit(mWedAttachTimer.IsRunning());
    mWedAttachTimer.Stop();

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
    LogInfo("HandleP2pLinkRequest: ExtAddress: %s, DeviceMode: %s",
            aRxInfo.mNeighbor->GetExtAddress().ToString().AsCString(),
            aRxInfo.mNeighbor->GetDeviceMode().ToString().AsCString());

    info.mLinkMargin = Get<Mac::Mac>().ComputeLinkMargin(aRxInfo.mMessage.GetAverageRss());

    SuccessOrExit(error = SendP2pLinkAcceptAndRequest(info));
    mWakeupTxScheduler.Stop();

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
        LogInfo("SendP2pLinkAcceptAndRequest --------------->");
    }
    else
    {
        LogInfo("SendP2pLinkAccept ------------------------->");
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
        mP2pEventCallback.InvokeIfSet(OT_P2P_EVENT_WC_CONNECTED, &aInfo.mExtAddress);
    }

    Log(kMessageSend, (command == kCommandLinkAccept) ? kTypeLinkAccept : kTypeLinkAcceptAndRequest, destination);

exit:
    FreeMessageOnError(message, error);
    return error;
}

void Mle::HandleP2pLinkAccept(RxInfo &aRxInfo)
{
    LogInfo("HandleP2pLinkAccept -------------------->");
    HandleP2pLinkAcceptVariant(aRxInfo, kTypeLinkAccept);
}

void Mle::HandleP2pLinkAcceptAndRequest(RxInfo &aRxInfo)
{
    LogInfo("HandleP2pLinkAcceptAndRequest ---------->");
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

    InitNeighbor(*peer, aRxInfo);

    peer->SetState(Neighbor::kStateValid);
    peer->GetLinkFrameCounters().SetAll(linkFrameCounter);
    peer->SetLinkAckFrameCounter(linkFrameCounter);
    peer->SetMleFrameCounter(mleFrameCounter);
    peer->SetState(Neighbor::kStateValid);
    peer->SetKeySequence(aRxInfo.mKeySequence);
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
        LogInfo("HandleP2pLinkAccept --------------------> P2P link is established !!!!!!!!");
        Get<MeshForwarder>().SetRxOnWhenIdle(IsRxOnWhenIdle());
        mP2pConnectedCallback.InvokeAndClearIfSet(kErrorNone);
        mP2pEventCallback.InvokeIfSet(OT_P2P_EVENT_WED_CONNECTED, &peer->GetExtAddress());
    }

exit:
    LogProcessError(aMessageType, error);
}

} // namespace Mle
} // namespace ot
#endif // OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE
