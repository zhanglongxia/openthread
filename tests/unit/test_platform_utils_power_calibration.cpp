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

#include <openthread/platform/radio.h>

#include "test_platform.h"
#include "test_util.h"
#include "utils/power_calibration.h"

namespace ot {

void TestPowerCalibration(void)
{
    otInstance *      instance;
    otRawPowerSetting settings;

    struct CalibratedPower
    {
        uint8_t           mChannel;
        int16_t           mActualPower;
        otRawPowerSetting mRawPowerSetting;
    };

    constexpr CalibratedPower kCalibratedPowerTable[] = {
        {11, 15000, {{0x02}, 1}},
        {11, 5000, {{0x00}, 1}},
        {11, 10000, {{0x01}, 1}},
    };

    instance = static_cast<otInstance *>(testInitInstance());
    VerifyOrQuit(instance != nullptr, "Null OpenThread instance");

    for (const CalibratedPower &calibratedPower : kCalibratedPowerTable)
    {
        SuccessOrQuit(otPlatRadioAddCalibratedPower(instance, calibratedPower.mChannel, calibratedPower.mActualPower,
                                                    &calibratedPower.mRawPowerSetting));
    }

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 4999));
    VerifyOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings) == OT_ERROR_NOT_FOUND);

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 5000));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x00);

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 9999));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x00);

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 10000));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x01);

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 14999));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x01);

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 15000));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x02);

    VerifyOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(12, &settings) == OT_ERROR_NOT_FOUND);

    SuccessOrQuit(otPlatRadioClearCalibratedPowers(instance));
    VerifyOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings) == OT_ERROR_NOT_FOUND);

    for (const CalibratedPower &calibratedPower : kCalibratedPowerTable)
    {
        SuccessOrQuit(otPlatRadioAddCalibratedPower(instance, calibratedPower.mChannel, calibratedPower.mActualPower,
                                                    &calibratedPower.mRawPowerSetting));
    }

    SuccessOrQuit(otPlatRadioSetChannelTargetPower(instance, 11, 15000));
    SuccessOrQuit(otUtilsPowerCalibrationGetRawPowerSetting(11, &settings));
    VerifyOrQuit(settings.m8[0] == 0x02);

    VerifyOrQuit(otPlatRadioAddCalibratedPower(instance, kCalibratedPowerTable[0].mChannel,
                                               kCalibratedPowerTable[0].mActualPower,
                                               &kCalibratedPowerTable[0].mRawPowerSetting) == OT_ERROR_INVALID_ARGS);

    testFreeInstance(instance);
}
} // namespace ot

int main(void)
{
    ot::TestPowerCalibration();
    printf("All tests passed\n");
    return 0;
}