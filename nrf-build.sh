#!/bin/bash

# 1. Prepare
#
# cd ot-nrf528xx
# rm -rf ./openthread
# ln -s /usr/local/google/home/zhanglongxia/repos/openthread/s2s/openthread ./openthread

# 2. Build
#
# ./nrf-build.sh && ./nrf-prog.sh

# 3. Run demo
#
# DUT=cli expect ./demo-wed.exp
# DUT=cli expect ./demo-wc.exp

# 4. Debug
#
# ./jlink-rtt.exp 683688378 // WC
# ./jlink-rtt.exp 683152435 19025 // WED

cd /usr/local/google/home/zhanglongxia/repos/openthread/nrf-wakeup/ot-nrf528xx && ./build.sh
