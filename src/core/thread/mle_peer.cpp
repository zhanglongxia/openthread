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

#include "instance/instance.hpp"
#include "openthread/platform/toolchain.h"
#include "utils/static_counter.hpp"

namespace ot {
namespace Mle {

RegisterLogModule("MleChild");

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
    LogInfo("SendP2pLinkRequest: DeviceMode: %s", GetDeviceMode().ToString().AsCString());

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

    LogInfo("HandleP2pLinkRequest: DeviceMode: %s", mode.ToString().AsCString());
    aRxInfo.mMessageInfo.GetPeerAddr().GetIid().ConvertToExtAddress(info.mExtAddress);
    ProcessKeySequence(aRxInfo);

    if (aRxInfo.mNeighbor == nullptr)
    {
        VerifyOrExit((aRxInfo.mNeighbor = Get<ChildTable>().GetNewChild(Child::kNeighborTypePeer)) != nullptr,
                     error = kErrorNoBufs);
    }

    InitNeighbor(*aRxInfo.mNeighbor, aRxInfo);
    aRxInfo.mNeighbor->SetDeviceMode(mode);
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
    return SendP2pLinkAcceptVariant(aInfo, false /* aIsLinkAcceptorRequest*/);
}

Error Mle::SendP2pLinkAcceptAndRequest(const LinkAcceptInfo &aInfo)
{
    return SendP2pLinkAcceptVariant(aInfo, true /* aIsLinkAcceptorRequest*/);
}

Error Mle::SendP2pLinkAcceptVariant(const LinkAcceptInfo &aInfo, bool aIsLinkAcceptorRequest)
{
    Error        error   = kErrorNone;
    TxMessage   *message = nullptr;
    Command      command = aIsLinkAcceptorRequest ? kCommandLinkAcceptAndRequest : kCommandLinkAccept;
    Ip6::Address destination;

    LogInfo("SendP2pLinkAcceptVariant ---------------> %u", command);

    VerifyOrExit((message = NewMleMessage(command)) != nullptr, error = kErrorNoBufs);
    if (command == kCommandLinkAcceptAndRequest)
    {
        SuccessOrExit(error = message->AppendModeTlv(GetDeviceMode()));
        SuccessOrExit(error = message->AppendVersionTlv());
        LogInfo("SendP2pLinkAcceptVariant: DeviceMode: %s", GetDeviceMode().ToString().AsCString());
    }

    SuccessOrExit(error = message->AppendResponseTlv(aInfo.mRxChallenge));
    if (command == kCommandLinkAcceptAndRequest)
    {
        Child *peer;

        VerifyOrExit((peer = Get<ChildTable>().FindChild(aInfo.mExtAddress, Child::kInStateLinkRequest)) != nullptr,
                     error = kErrorNotFound);
        peer->GenerateChallenge();
        SuccessOrExit(error = message->AppendChallengeTlv(peer->GetChallenge()));
    }

    SuccessOrExit(error = message->AppendLinkMarginTlv(aInfo.mLinkMargin));
    SuccessOrExit(error = message->AppendLinkAndMleFrameCounterTlvs());
    // SuccessOrExit(error = message->AppendCslClockAccuracyTlv());

    message->SetDirectTransmission();

    destination.SetToLinkLocalAddress(aInfo.mExtAddress);

    SuccessOrExit(error = message->SendTo(destination));

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

    LogInfo("HandleP2pLinkAcceptVariant TP1");

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
        LogInfo("HandleP2pLinkAcceptVariant: DeviceMode: %s", mode.ToString().AsCString());
        LogInfo("HandleP2pLinkAcceptVariant1: ExtAddress: %s, DeviceMode: %s",
                peer->GetExtAddress().ToString().AsCString(), peer->GetDeviceMode().ToString().AsCString());
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
    // peer->SetLinkQualityOut(LinkQualityForLinkMargin(linkMargin));
    aRxInfo.mClass = RxInfo::kAuthoritativeMessage;

    LogInfo("HandleP2pLinkAcceptVariant2: ExtAddress: %s, DeviceMode: %s", peer->GetExtAddress().ToString().AsCString(),
            peer->GetDeviceMode().ToString().AsCString());

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
        mWakeupCallback.InvokeAndClearIfSet(kErrorNone);
    }

    LogInfo("HandleP2pLinkAcceptVariant TP5");

exit:
    LogProcessError(aMessageType, error);
}

} // namespace Mle
} // namespace ot
#endif // OPENTHREAD_CONFIG_PEER_TO_PEER_ENABLE
