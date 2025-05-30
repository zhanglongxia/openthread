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
 *   This file implements the Joiner Router role.
 */

#include "joiner_router.hpp"

#if OPENTHREAD_FTD

#include "instance/instance.hpp"

namespace ot {
namespace MeshCoP {

RegisterLogModule("JoinerRouter");

JoinerRouter::JoinerRouter(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mSocket(aInstance, *this)
    , mTimer(aInstance)
    , mJoinerUdpPort(0)
    , mIsJoinerPortConfigured(false)
{
}

void JoinerRouter::HandleNotifierEvents(Events aEvents)
{
    if (aEvents.Contains(kEventThreadNetdataChanged))
    {
        Start();
    }
}

void JoinerRouter::Start(void)
{
    VerifyOrExit(Get<Mle::Mle>().IsFullThreadDevice());

    if (Get<NetworkData::Leader>().IsJoiningAllowed())
    {
        uint16_t port = GetJoinerUdpPort();

        VerifyOrExit(!mSocket.IsBound());

        IgnoreError(mSocket.Open(Ip6::kNetifThreadInternal));
        IgnoreError(mSocket.Bind(port));
        IgnoreError(Get<Ip6::Filter>().AddUnsecurePort(port));
        LogInfo("Joiner Router: start");
    }
    else
    {
        VerifyOrExit(mSocket.IsBound());

        IgnoreError(Get<Ip6::Filter>().RemoveUnsecurePort(mSocket.GetSockName().mPort));

        IgnoreError(mSocket.Close());
    }

exit:
    return;
}

uint16_t JoinerRouter::GetJoinerUdpPort(void) const
{
    uint16_t port;

    if (mIsJoinerPortConfigured)
    {
        ExitNow(port = mJoinerUdpPort);
    }

    if (Get<NetworkData::Leader>().FindJoinerUdpPort(port) == kErrorNone)
    {
        ExitNow();
    }

    port = kDefaultJoinerUdpPort;

exit:
    return port;
}

void JoinerRouter::SetJoinerUdpPort(uint16_t aJoinerUdpPort)
{
    mJoinerUdpPort          = aJoinerUdpPort;
    mIsJoinerPortConfigured = true;
    Start();
}

void JoinerRouter::HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    Error            error;
    Coap::Message   *message = nullptr;
    Tmf::MessageInfo messageInfo(GetInstance());
    ExtendedTlv      tlv;
    uint16_t         borderAgentRloc;
    OffsetRange      offsetRange;

    LogInfo("JoinerRouter::HandleUdpReceive");

    SuccessOrExit(error = Get<NetworkData::Leader>().FindBorderAgentRloc(borderAgentRloc));

    message = Get<Tmf::Agent>().NewPriorityNonConfirmablePostMessage(kUriRelayRx);
    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

    SuccessOrExit(error = Tlv::Append<JoinerUdpPortTlv>(*message, aMessageInfo.GetPeerPort()));
    SuccessOrExit(error = Tlv::Append<JoinerIidTlv>(*message, aMessageInfo.GetPeerAddr().GetIid()));
    SuccessOrExit(error = Tlv::Append<JoinerRouterLocatorTlv>(*message, Get<Mle::Mle>().GetRloc16()));

    offsetRange.InitFromMessageOffsetToEnd(aMessage);

    tlv.SetType(Tlv::kJoinerDtlsEncapsulation);
    tlv.SetLength(offsetRange.GetLength());
    SuccessOrExit(error = message->Append(tlv));
    SuccessOrExit(error = message->AppendBytesFromMessage(aMessage, offsetRange));

    messageInfo.SetSockAddrToRlocPeerAddrTo(borderAgentRloc);

    SuccessOrExit(error = Get<Tmf::Agent>().SendMessage(*message, messageInfo));

    LogInfo("Sent %s", UriToString<kUriRelayRx>());

exit:
    FreeMessageOnError(message, error);
}

template <> void JoinerRouter::HandleTmf<kUriRelayTx>(Coap::Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    OT_UNUSED_VARIABLE(aMessageInfo);

    Error                    error;
    uint16_t                 joinerPort;
    Ip6::InterfaceIdentifier joinerIid;
    Kek                      kek;
    OffsetRange              offsetRange;
    Message                 *message = nullptr;
    Message::Settings        settings(kNoLinkSecurity, Message::kPriorityNet);
    Ip6::MessageInfo         messageInfo;

    VerifyOrExit(aMessage.IsNonConfirmablePostRequest(), error = kErrorDrop);

    LogInfo("Received %s", UriToString<kUriRelayTx>());

    SuccessOrExit(error = Tlv::Find<JoinerUdpPortTlv>(aMessage, joinerPort));
    SuccessOrExit(error = Tlv::Find<JoinerIidTlv>(aMessage, joinerIid));

    SuccessOrExit(error = Tlv::FindTlvValueOffsetRange(aMessage, Tlv::kJoinerDtlsEncapsulation, offsetRange));

    VerifyOrExit((message = mSocket.NewMessage(0, settings)) != nullptr, error = kErrorNoBufs);

    SuccessOrExit(error = message->AppendBytesFromMessage(aMessage, offsetRange));

    messageInfo.GetPeerAddr().SetToLinkLocalAddress(joinerIid);
    messageInfo.SetPeerPort(joinerPort);

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    if (Tlv::Find<JoinerRouterKekTlv>(aMessage, kek) == kErrorNone)
    {
        LogInfo("Received kek");

        DelaySendingJoinerEntrust(messageInfo, kek);
    }

exit:
    FreeMessageOnError(message, error);
}

void JoinerRouter::DelaySendingJoinerEntrust(const Ip6::MessageInfo &aMessageInfo, const Kek &aKek)
{
    Error                 error   = kErrorNone;
    Message              *message = Get<MessagePool>().Allocate(Message::kTypeOther);
    JoinerEntrustMetadata metadata;

    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

    metadata.mMessageInfo = aMessageInfo;
    metadata.mMessageInfo.SetPeerPort(Tmf::kUdpPort);
    metadata.mSendTime = TimerMilli::GetNow() + kJoinerEntrustTxDelay;
    metadata.mKek      = aKek;

    SuccessOrExit(error = metadata.AppendTo(*message));

    mDelayedJoinEnts.Enqueue(*message);

    if (!mTimer.IsRunning())
    {
        mTimer.FireAt(metadata.mSendTime);
    }

exit:
    FreeMessageOnError(message, error);
    LogWarnOnError(error, "schedule joiner entrust");
}

void JoinerRouter::HandleTimer(void) { SendDelayedJoinerEntrust(); }

void JoinerRouter::SendDelayedJoinerEntrust(void)
{
    JoinerEntrustMetadata metadata;
    Message              *message = mDelayedJoinEnts.GetHead();

    VerifyOrExit(message != nullptr);
    VerifyOrExit(!mTimer.IsRunning());

    metadata.ReadFrom(*message);

    if (TimerMilli::GetNow() < metadata.mSendTime)
    {
        mTimer.FireAt(metadata.mSendTime);
    }
    else
    {
        mDelayedJoinEnts.DequeueAndFree(*message);

        Get<KeyManager>().SetKek(metadata.mKek);

        if (SendJoinerEntrust(metadata.mMessageInfo) != kErrorNone)
        {
            mTimer.Start(0);
        }
    }

exit:
    return;
}

Error JoinerRouter::SendJoinerEntrust(const Ip6::MessageInfo &aMessageInfo)
{
    Error          error = kErrorNone;
    Coap::Message *message;

    message = PrepareJoinerEntrustMessage();
    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

    IgnoreError(Get<Tmf::Agent>().AbortTransaction(&JoinerRouter::HandleJoinerEntrustResponse, this));

    SuccessOrExit(error = Get<Tmf::Agent>().SendMessage(*message, aMessageInfo,
                                                        &JoinerRouter::HandleJoinerEntrustResponse, this));

    LogInfo("Sent %s (len= %d)", UriToString<kUriJoinerEntrust>(), message->GetLength());
    LogCert("[THCI] direction=send | type=JOIN_ENT.ntf");

exit:
    FreeMessageOnError(message, error);
    return error;
}

Coap::Message *JoinerRouter::PrepareJoinerEntrustMessage(void)
{
    static const Tlv::Type kTlvTypes[] = {
        Tlv::kNetworkKey,      Tlv::kMeshLocalPrefix, Tlv::kExtendedPanId, Tlv::kNetworkName,
        Tlv::kActiveTimestamp, Tlv::kChannelMask,     Tlv::kPskc,          Tlv::kSecurityPolicy,
    };

    Error          error   = kErrorNone;
    Coap::Message *message = nullptr;
    Dataset        dataset;

    message = Get<Tmf::Agent>().NewPriorityConfirmablePostMessage(kUriJoinerEntrust);
    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

    message->SetSubType(Message::kSubTypeJoinerEntrust);

    SuccessOrExit(error = Get<ActiveDatasetManager>().Read(dataset));

    for (Tlv::Type tlvType : kTlvTypes)
    {
        const Tlv *tlv = dataset.FindTlv(tlvType);

        VerifyOrExit(tlv != nullptr, error = kErrorInvalidState);
        SuccessOrExit(error = tlv->AppendTo(*message));
    }

    SuccessOrExit(error = Tlv::Append<NetworkKeySequenceTlv>(*message, Get<KeyManager>().GetCurrentKeySequence()));

exit:
    FreeAndNullMessageOnError(message, error);
    return message;
}

void JoinerRouter::HandleJoinerEntrustResponse(void                *aContext,
                                               otMessage           *aMessage,
                                               const otMessageInfo *aMessageInfo,
                                               otError              aResult)
{
    static_cast<JoinerRouter *>(aContext)->HandleJoinerEntrustResponse(AsCoapMessagePtr(aMessage),
                                                                       AsCoreTypePtr(aMessageInfo), aResult);
}

void JoinerRouter::HandleJoinerEntrustResponse(Coap::Message          *aMessage,
                                               const Ip6::MessageInfo *aMessageInfo,
                                               Error                   aResult)
{
    OT_UNUSED_VARIABLE(aMessageInfo);

    SendDelayedJoinerEntrust();

    VerifyOrExit(aResult == kErrorNone && aMessage != nullptr);

    VerifyOrExit(aMessage->GetCode() == Coap::kCodeChanged);

    LogInfo("Receive %s response", UriToString<kUriJoinerEntrust>());
    LogCert("[THCI] direction=recv | type=JOIN_ENT.rsp");

exit:
    return;
}

} // namespace MeshCoP
} // namespace ot

#endif // OPENTHREAD_FTD
