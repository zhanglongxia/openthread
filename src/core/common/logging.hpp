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
 *   This file includes logging related macro/function definitions.
 */

#ifndef LOGGING_HPP_
#define LOGGING_HPP_

#include <stdio.h>
#include <openthread/logging.h>
#include <openthread/platform/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OT_LOG_TAG
#define OT_LOG_TAG "OT"
#endif

void otDump(otLogLevel aLogLevel, const char *aLogTag, const char *aId, const void *aBuf, const size_t aLength);

#define otLogResult(aError, aFormat, ...)                                                                   \
    do                                                                                                      \
    {                                                                                                       \
        otError _err = (aError);                                                                            \
        otLog(_err == OT_ERROR_NONE ? OT_LOG_LEVEL_INFO : OT_LOG_LEVEL_WARNING, OT_LOG_TAG, aFormat ": %s", \
              ##__VA_ARGS__, otThreadErrorToString(_err));                                                  \
    } while (0)

#define otLogCrit(aFormat, ...) otLog(OT_LOG_LEVEL_CRIT, OT_LOG_TAG, aFormat, ##__VA_ARGS__)
#define otLogWarn(aFormat, ...) otLog(OT_LOG_LEVEL_WARN, OT_LOG_TAG, aFormat, ##__VA_ARGS__)
#define otLogNote(aFormat, ...) otLog(OT_LOG_LEVEL_NOTE, OT_LOG_TAG, aFormat, ##__VA_ARGS__)
#define otLogInfo(aFormat, ...) otLog(OT_LOG_LEVEL_INFO, OT_LOG_TAG, aFormat, ##__VA_ARGS__)
#define otLogDebg(aFormat, ...) otLog(OT_LOG_LEVEL_DEBG, OT_LOG_TAG, aFormat, ##__VA_ARGS__)

#define otDumpCrit(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_CRIT, OT_LOG_TAG, aId, aBuf, aLength)
#define otDumpWarn(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_WARN, OT_LOG_TAG, aId, aBuf, aLength)
#define otDumpNote(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_NOTE, OT_LOG_TAG, aId, aBuf, aLength)
#define otDumpInfo(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_INFO, OT_LOG_TAG, aId, aBuf, aLength)
#define otDumpDebg(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_DEBG, OT_LOG_TAG, aId, aBuf, aLength)

#if OPENTHREAD_CONFIG_REFERENCE_DEVICE_ENABLE
#define otDumpCertMeshCoP(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_NONE, OT_LOG_TAG, aId, aBuf, aLength)
#define otDumpCertBr(aId, aBuf, aLength) otDump(OT_LOG_LEVEL_NONE, OT_LOG_TAG, aId, aBuf, aLength)
#define otLogCertMeshCoP(aFormat, ...) otLog(OT_LOG_LEVEL_CRIT, OT_LOG_TAG, aFormat, ##__VA_ARGS__)
#else
#define otDumpCertMeshCoP(aId, aBuf, aLength)
#define otDumpCertBr(aId, aBuf, aLength)
#define otLogCertMeshCoP(aFormat, ...)
#endif

#define otLogMac(aLogLevel, aFormat, ...) otLog(aLogLevel, OT_LOG_TAG, aFormat, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGING_HPP_
