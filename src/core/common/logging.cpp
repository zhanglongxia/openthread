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
 *   This file implements the logging related functions.
 */

#include "openthread-core-config.h"

#include <openthread/logging.h>

#include "logging.hpp"

#include "common/code_utils.hpp"
#include "common/instance.hpp"
#include "common/string.hpp"

/*
 * Verify debug uart dependency.
 *
 * It is reasonable to only enable the debug uart and not enable logs to the DEBUG uart.
 */
#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART) && (!OPENTHREAD_CONFIG_ENABLE_DEBUG_UART)
#error OPENTHREAD_CONFIG_ENABLE_DEBUG_UART_LOG requires OPENTHREAD_CONFIG_ENABLE_DEBUG_UART
#endif

#ifdef __cplusplus
extern "C" {
#endif

static const char *LogLevelToString(otLogLevel aLogLevel)
{
    const char *str = "";

#if OPENTHREAD_CONFIG_LOG_PREPEND_LEVEL
    static const char *const kLevelStrings[] = {"[NONE]", "[CRIT]", "[WARN]", "[NOTE]", "[INFO]", "[DEBG]"};

    str = ((aLogLevel >= 0) && (aLogLevel < static_cast<int>(OT_ARRAY_LENGTH(kLevelStrings))))
              ? kLevelStrings[aLogLevel]
              : "";
#else
    OT_UNUSED_VARIABLE(aLogLevel);
#endif

    return str;
}

static const char *LogTagToPrefix(const char *aLogTag)
{
    const char *ptr = "";

#if OPENTHREAD_CONFIG_LOG_PREPEND_REGION
    // Log prefix format : "-xxxxxxx-: "
    const uint8_t kMinTagSize = 7;
    const uint8_t kMaxTagSize = 32;
    const uint8_t kBufferSize = kMaxTagSize + 5;
    static char   prefix[kBufferSize];
    uint8_t       tagLength = strlen(aLogTag) > kMaxTagSize ? kMaxTagSize : strlen(aLogTag);
    int           index     = 0;

    if (strlen(aLogTag) > 0)
    {
        uint8_t numHyphens = tagLength < kMinTagSize ? kMinTagSize - tagLength : 0;

        prefix[0] = '-';
        memcpy(&prefix[1], aLogTag, tagLength);

        index = tagLength + 1;

        memset(&prefix[index], '-', numHyphens + 1);
        index += numHyphens + 1;
    }

    prefix[index++] = ':';
    prefix[index++] = ' ';
    prefix[index++] = '\0';
    ptr             = prefix;
#else
    OT_UNUSED_VARIABLE(aLogTag);
#endif

    return ptr;
}

static void Log(otLogLevel aLogLevel, const char *aLogTag, const char *aFormat, va_list aArgs)
{
    otLogLevel level = OPENTHREAD_CONFIG_LOG_LEVEL;
    ot::String<OPENTHREAD_CONFIG_LOG_MAX_SIZE> logString;

#if OPENTHREAD_CONFIG_LOG_LEVEL_DYNAMIC_ENABLE
    level = otLoggingGetLevel();
#endif

    // VerifyOrExit(TAG_ENABLE(OT_TAG_ENABLE(OT_LOG_TAG)) >= 1);

    VerifyOrExit(aLogLevel <= level);

    logString.Append("%s", LogLevelToString(aLogLevel));
    logString.Append("%s", LogTagToPrefix(aLogTag));
    logString.AppendVarArgs(aFormat, aArgs);
    otPlatLog(aLogLevel, (otLogRegion)(0), "%s", logString.AsCString());

exit:
    return;
}

void otLog(otLogLevel aLevel, const char *aLogTag, const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    Log(aLevel, aLogTag, aFormat, ap);
    va_end(ap);
}

#if OPENTHREAD_CONFIG_LOG_PKT_DUMP
enum : uint8_t
{
    kStringLineLength = 80,
    kDumpBytesPerLine = 16,
};

static void DumpLine(otLogLevel aLogLevel, const char *aLogTag, const uint8_t *aBytes, const size_t aLength)
{
    ot::String<kStringLineLength> string;
    ot::StringWriter              writer(string);

    writer.Append("|");

    for (uint8_t i = 0; i < kDumpBytesPerLine; i++)
    {
        if (i < aLength)
        {
            writer.Append(" %02X", aBytes[i]);
        }
        else
        {
            writer.Append(" ..");
        }

        if (!((i + 1) % 8))
        {
            writer.Append(" |");
        }
    }

    writer.Append(" ");

    for (uint8_t i = 0; i < kDumpBytesPerLine; i++)
    {
        char c = '.';

        if (i < aLength)
        {
            char byteAsChar = static_cast<char>(0x7f & aBytes[i]);

            if (isprint(byteAsChar))
            {
                c = byteAsChar;
            }
        }

        writer.Append("%c", c);
    }

    otLog(aLogLevel, aLogTag, "%s", string.AsCString());
}

void otDump(otLogLevel aLogLevel, const char *aLogTag, const char *aId, const void *aBuf, const size_t aLength)
{
    enum : uint8_t
    {
        kWidth = 72,
    };

    size_t                        idLen = strlen(aId);
    ot::String<kStringLineLength> string;
    ot::StringWriter              writer(string);

    for (size_t i = 0; i < (kWidth - idLen) / 2 - 5; i++)
    {
        writer.Append("=");
    }

    writer.Append("[%s len=%03u]", aId, static_cast<unsigned>(aLength));

    for (size_t i = 0; i < (kWidth - idLen) / 2 - 4; i++)
    {
        writer.Append("=");
    }

    otLog(aLogLevel, aLogTag, "%s", string.AsCString());

    for (size_t i = 0; i < aLength; i += kDumpBytesPerLine)
    {
        DumpLine(aLogLevel, aLogTag, static_cast<const uint8_t *>(aBuf) + i,
                 OT_MIN((aLength - i), static_cast<size_t>(kDumpBytesPerLine)));
    }

    writer.Clear();

    for (size_t i = 0; i < kWidth; i++)
    {
        writer.Append("-");
    }

    otLog(aLogLevel, aLogTag, "%s", string.AsCString());
}
#else  // OPENTHREAD_CONFIG_LOG_PKT_DUMP
void otDump(otLogLevel, const char *, const char *, const void *, const size_t)
{
}
#endif // OPENTHREAD_CONFIG_LOG_PKT_DUMP

#if OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_NONE
/* this provides a stub, in case something uses the function */
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);
}
#endif

OT_TOOL_WEAK void otPlatLogLine(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aLogLine)
{
    otPlatLog(aLogLevel, aLogRegion, "%s", aLogLine);
}

#ifdef __cplusplus
}
#endif
