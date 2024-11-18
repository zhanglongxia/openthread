#!/bin/bash
set -xeuo pipefail

./script/cmake-build simulation -DOT_LOG_OUTPUT=PLATFORM_DEFINED -DOT_LOG_LEVEL=DEBG -DOPENTHREAD_CONFIG_LOG_MAX_SIZE=750 -DOPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_ENABLE=1 -DOT_CHANNEL_MONITOR=OFF -DOT_CHANNEL_MANAGER=OFF -DOT_WAKEUP_COORDINATOR=ON -DOT_FTD=ON
cp ./build/simulation/examples/apps/cli/ot-cli-ftd ./build/simulation/examples/apps/cli/ot-cli-ftd-wc

./script/cmake-build simulation -DOT_LOG_OUTPUT=PLATFORM_DEFINED -DOT_LOG_LEVEL=DEBG -DOPENTHREAD_CONFIG_LOG_MAX_SIZE=750 -DOPENTHREAD_CONFIG_CLI_LOG_INPUT_OUTPUT_ENABLE=1 -DOT_CHANNEL_MONITOR=OFF -DOT_CHANNEL_MANAGER=OFF -DOT_WAKEUP_END_DEVICE=ON -DOT_FTD=OFF 
cp ./build/simulation/examples/apps/cli/ot-cli-mtd ./build/simulation/examples/apps/cli/ot-cli-mtd-wed
