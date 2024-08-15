/*
 *  Copyright (c) 2024, The OpenThread Authors.
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
 *   This file includes the implementation for the socket interface to radio (RCP).
 */

#include "socket_interface.hpp"

#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <openthread/logging.h>

#include "platform-posix.h"
#include "common/code_utils.hpp"
#include "openthread/openthread-system.h"

namespace ot {
namespace Posix {

const char SocketInterface::kLogModuleName[] = "SocketIntface";

SocketInterface::SocketInterface(const ot::Url::Url &aRadioUrl)
    : mReceiveFrameCallback(nullptr)
    , mReceiveFrameContext(nullptr)
    , mReceiveFrameBuffer(nullptr)
    , mSockFd(-1)
    , mRadioUrl(aRadioUrl)
    , mIsHardwareResetting(false)
{
    memset(&mInterfaceMetrics, 0, sizeof(mInterfaceMetrics));
    mInterfaceMetrics.mRcpInterfaceType = kSpinelInterfaceTypeSocket;
}

otError SocketInterface::Init(ReceiveFrameCallback aCallback, void *aCallbackContext, RxFrameBuffer &aFrameBuffer)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mSockFd == -1, error = OT_ERROR_ALREADY);

    WaitForSocketFileCreated(mRadioUrl.GetPath());

    mSockFd = OpenFile(mRadioUrl);
    VerifyOrExit(mSockFd != -1, error = OT_ERROR_FAILED);

    mReceiveFrameCallback = aCallback;
    mReceiveFrameContext  = aCallbackContext;
    mReceiveFrameBuffer   = &aFrameBuffer;

exit:
    return error;
}

SocketInterface::~SocketInterface(void) { Deinit(); }

void SocketInterface::Deinit(void)
{
    CloseFile();

    mReceiveFrameCallback = nullptr;
    mReceiveFrameContext  = nullptr;
    mReceiveFrameBuffer   = nullptr;
}

otError SocketInterface::SendFrame(const uint8_t *aFrame, uint16_t aLength)
{
    Write(aFrame, aLength);

    return OT_ERROR_NONE;
}

struct timeval SocketInterface::UsToTimeval(uint64_t aTimeoutUs) const
{
    struct timeval time = {static_cast<time_t>(aTimeoutUs / US_PER_S), static_cast<suseconds_t>(aTimeoutUs % US_PER_S)};
    return time;
}

otError SocketInterface::WaitForFrame(uint64_t aTimeoutUs)
{
    otError        error   = OT_ERROR_NONE;
    struct timeval timeout = UsToTimeval(aTimeoutUs);

    fd_set readFds;
    fd_set errorFds;
    int    rval;

    FD_ZERO(&readFds);
    FD_ZERO(&errorFds);
    FD_SET(mSockFd, &readFds);
    FD_SET(mSockFd, &errorFds);

    rval = TEMP_FAILURE_RETRY(select(mSockFd + 1, &readFds, nullptr, &errorFds, &timeout));

    if (rval > 0)
    {
        if (FD_ISSET(mSockFd, &readFds))
        {
            Read();
        }
        else if (FD_ISSET(mSockFd, &errorFds))
        {
            DieNowWithMessage("RCP error", OT_EXIT_FAILURE);
        }
        else
        {
            DieNow(OT_EXIT_FAILURE);
        }
    }
    else if (rval == 0)
    {
        ExitNow(error = OT_ERROR_RESPONSE_TIMEOUT);
    }
    else
    {
        DieNowWithMessage("Wait response", OT_EXIT_FAILURE);
    }

exit:
    return error;
}

otError SocketInterface::WaitForHardwareResetCompletion(uint32_t aTimeoutMs)
{
    otError error   = OT_ERROR_NONE;
    int     retries = 0;
    int     rval;

    while (mIsHardwareResetting && (retries++ < kMaxRetriesForSocketCloseCheck))
    {
        struct timeval timeout = UsToTimeval(aTimeoutMs * US_PER_MS);
        fd_set         readFds;

        FD_ZERO(&readFds);
        FD_SET(mSockFd, &readFds);

        rval = TEMP_FAILURE_RETRY(select(mSockFd + 1, &readFds, nullptr, nullptr, &timeout));

        if (rval > 0)
        {
            Read();
        }
        else if (rval < 0)
        {
            DieNowWithMessage("Wait response", OT_EXIT_ERROR_ERRNO);
        }
        else
        {
            LogInfo("Waiting for hardware reset, retry attempt: %d, max attempt: %d", retries,
                    kMaxRetriesForSocketCloseCheck);
        }
    }

    VerifyOrExit(!mIsHardwareResetting, error = OT_ERROR_FAILED);

    WaitForSocketFileCreated(mRadioUrl.GetPath());
    mSockFd = OpenFile(mRadioUrl);
    VerifyOrExit(mSockFd != -1, error = OT_ERROR_FAILED);

exit:
    return error;
}

void SocketInterface::UpdateFdSet(void *aMainloopContext)
{
    otSysMainloopContext *context = reinterpret_cast<otSysMainloopContext *>(aMainloopContext);

    assert(context != nullptr);

    FD_SET(mSockFd, &context->mReadFdSet);

    if (context->mMaxFd < mSockFd)
    {
        context->mMaxFd = mSockFd;
    }
}

void SocketInterface::Process(const void *aMainloopContext)
{
    const otSysMainloopContext *context = reinterpret_cast<const otSysMainloopContext *>(aMainloopContext);

    assert(context != nullptr);

    if (FD_ISSET(mSockFd, &context->mReadFdSet))
    {
        Read();
    }
}

otError SocketInterface::HardwareReset(void)
{
    uint8_t resetCommand[] = {SPINEL_HEADER_FLAG, SPINEL_CMD_RESET, SPINEL_RESET_HARDWARE};

    mIsHardwareResetting = true;
    SendFrame(resetCommand, sizeof(resetCommand));

    return WaitForHardwareResetCompletion(kMaxSelectTimeMs);
}

void SocketInterface::Read(void)
{
    uint8_t buffer[kMaxFrameSize];

    ssize_t rval = TEMP_FAILURE_RETRY(read(mSockFd, buffer, sizeof(buffer)));

    if (rval > 0)
    {
        ProcessReceivedData(buffer, static_cast<uint16_t>(rval));
    }
    else if (rval < 0)
    {
        DieNow(OT_EXIT_ERROR_ERRNO);
    }
    else
    {
        if (mIsHardwareResetting)
        {
            mIsHardwareResetting = false;
            mSockFd              = -1;
            LogInfo("Socket connection is closed due to hardware reset.");
        }
        else
        {
            LogCrit("Socket connection is closed by remote.");
            exit(OT_EXIT_FAILURE);
        }
    }
}

void SocketInterface::Write(const uint8_t *aFrame, uint16_t aLength)
{
    ssize_t rval = TEMP_FAILURE_RETRY(write(mSockFd, aFrame, aLength));
    VerifyOrDie(rval >= 0, OT_EXIT_ERROR_ERRNO);
    VerifyOrDie(rval > 0, OT_EXIT_FAILURE);
}

void SocketInterface::ProcessReceivedData(const uint8_t *aBuffer, uint16_t aLength)
{
    while (aLength--)
    {
        uint8_t byte = *aBuffer++;
        if (mReceiveFrameBuffer->CanWrite(sizeof(uint8_t)))
        {
            IgnoreError(mReceiveFrameBuffer->WriteByte(byte));
        }
        else
        {
            HandleSocketFrame(this, OT_ERROR_NO_BUFS);
            return;
        }
    }
    HandleSocketFrame(this, OT_ERROR_NONE);
}

void SocketInterface::HandleSocketFrame(void *aContext, otError aError)
{
    static_cast<SocketInterface *>(aContext)->HandleSocketFrame(aError);
}

void SocketInterface::HandleSocketFrame(otError aError)
{
    VerifyOrExit((mReceiveFrameCallback != nullptr) && (mReceiveFrameBuffer != nullptr));

    if (aError == OT_ERROR_NONE)
    {
        mReceiveFrameCallback(mReceiveFrameContext);
    }
    else
    {
        mReceiveFrameBuffer->DiscardFrame();
        LogWarn("Process socket frame failed: %s", otThreadErrorToString(aError));
    }

exit:
    return;
}

int SocketInterface::OpenFile(const ot::Url::Url &aRadioUrl)
{
    int         fd = -1;
    sockaddr_un serverAddress;

    VerifyOrExit(sizeof(serverAddress.sun_path) > strlen(aRadioUrl.GetPath()), LogCrit("Invalid file path length"));
    strncpy(serverAddress.sun_path, aRadioUrl.GetPath(), sizeof(serverAddress.sun_path));
    serverAddress.sun_family = AF_UNIX;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    VerifyOrExit(fd != -1, LogCrit("open(): errno=%s", strerror(errno)));

    if (connect(fd, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        LogCrit("connect(): errno=%s", strerror(errno));
        close(fd);
        fd = -1;
    }

exit:
    return fd;
}

void SocketInterface::CloseFile(void)
{
    VerifyOrExit(mSockFd != -1);

    VerifyOrExit(0 == close(mSockFd), LogCrit("close(): errno=%s", strerror(errno)));
    VerifyOrExit(wait(nullptr) != -1 || errno == ECHILD, LogCrit("wait(): errno=%s", strerror(errno)));

    mSockFd = -1;

exit:
    return;
}

void SocketInterface::WaitForSocketFileCreated(const char *aPath)
{
    int   inotifyFd;
    int   wd;
    char *path      = strdup(aPath);
    char *directory = dirname(path);

    VerifyOrDie(path != nullptr, OT_EXIT_ERROR_ERRNO);
    VerifyOrExit(!IsSocketFileExisted(aPath));

    inotifyFd = inotify_init();
    VerifyOrDie(inotifyFd != -1, OT_EXIT_ERROR_ERRNO);
    wd = inotify_add_watch(inotifyFd, directory, IN_CREATE);
    VerifyOrDie(wd != -1, OT_EXIT_ERROR_ERRNO);

    LogInfo("Waiting for socket file %s be created...", aPath);

    while (true)
    {
        struct timeval timeout = UsToTimeval(kMaxSelectTimeMs * US_PER_MS);
        fd_set         fds;
        FD_ZERO(&fds);
        FD_SET(inotifyFd, &fds);

        int rval = select(inotifyFd + 1, &fds, nullptr, nullptr, &timeout);
        VerifyOrDie(rval >= 0, OT_EXIT_ERROR_ERRNO);

        if (rval == 0 && IsSocketFileExisted(aPath))
        {
            break;
        }

        if (FD_ISSET(inotifyFd, &fds))
        {
            char    buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
            ssize_t bytesRead = read(inotifyFd, buffer, sizeof(buffer));

            VerifyOrDie(bytesRead >= 0, OT_EXIT_ERROR_ERRNO);

            struct inotify_event *event = reinterpret_cast<struct inotify_event *>(buffer);
            if ((event->mask & IN_CREATE) && IsSocketFileExisted(aPath))
            {
                break;
            }
        }
    }

    close(inotifyFd);

exit:
    free(path);
    LogInfo("Socket file: %s is created", aPath);
    return;
}

bool SocketInterface::IsSocketFileExisted(const char *aPath)
{
    struct stat st;
    return stat(aPath, &st) == 0 && S_ISSOCK(st.st_mode);
}

} // namespace Posix
} // namespace ot
