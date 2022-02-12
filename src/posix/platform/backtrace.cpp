/*
 *  Copyright (c) 2022, The OpenThread Authors.
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
 *   This file implements backtrace for posix.
 *
 */

#include "platform-posix.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#if OPENTHREAD_POSIX_CONFIG_BACKTRACE_ENABLE

#ifdef __ANDROID__
#include <cxxabi.h>
#include <dlfcn.h>
#include <unwind.h>

struct BacktraceState
{
    void **current;
    void **end;
};

_Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context *context, void *arg)
{
    BacktraceState *state = (BacktraceState *)arg;
    uintptr_t       pc    = _Unwind_GetIP(context);

    if (pc)
    {
        if (state->current == state->end)
        {
            return _URC_END_OF_STACK;
        }
        else
        {
            *state->current++ = reinterpret_cast<void *>(pc);
        }
    }

    return _URC_NO_REASON;
}

static void signalHandler(int signo)
{
    const uint8_t  kBacktraceStackDepth = 50;
    void *         stackBuffer[kBacktraceStackDepth];
    BacktraceState state;

    otLogCritPlat(" *** FATAL ERROR: Caught signal %d (%s):", signo, strsignal(signo));

    state.current = stackBuffer;
    state.end     = stackBuffer + kBacktraceStackDepth;

    _Unwind_Backtrace(android_unwind_callback, &state);

    int count = static_cast<int>(state.current - stackBuffer);

    for (int i = 0; i < count; i++)
    {
        const void *addr   = stackBuffer[i];
        const char *symbol = "";
        Dl_info     info;
        int         status = 0;
        char *      demangled;

        if (dladdr(addr, &info) && info.dli_sname)
        {
            symbol = info.dli_sname;
        }

        demangled = __cxxabiv1::__cxa_demangle(symbol, 0, 0, &status);

        otLogCritPlat("Backtrace %2d: %s 0x%p", i, ((demangled != nullptr) && (status == 0)) ? demangled : symbol,
                      addr);

        if (demangled != nullptr)
        {
            free(demangled);
        }
    }
}
#else
#include <execinfo.h>
static void signalHandler(int signo)
{
    const uint8_t kBacktraceStackDepth = 50;
    void *        stackBuffer[kBacktraceStackDepth];
    void **       stack = stackBuffer;
    char **       stackSymbols;
    int           stackDepth;

    stackDepth = backtrace(stack, kBacktraceStackDepth);

    stackSymbols = backtrace_symbols(stack, stackDepth);
    VerifyOrExit(stackSymbols != nullptr);

    fprintf(stderr, " *** FATAL ERROR: Caught signal %d (%s):\n", signo, strsignal(signo));
    otLogCritPlat(" *** FATAL ERROR: Caught signal %d (%s):", signo, strsignal(signo));

    for (int i = 0; i < stackDepth; i++)
    {
        fprintf(stderr, "Backtrace %2d: %s\n", i, stackSymbols[i]);
        otLogCritPlat("Backtrace %2d: %s\n", i, stackSymbols[i]);
    }

    free(stackSymbols);

exit:
    exit(EXIT_FAILURE);
}
#endif

void platformBacktraceInit(void)
{
    signal(SIGABRT, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGSYS, signalHandler);
    signal(SIGPIPE, signalHandler);
}
#endif // OPENTHREAD_POSIX_CONFIG_BACKTRACE_ENABLE
