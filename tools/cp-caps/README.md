# RCP Capabilities Test

This test is used for testing RCP capabilities.

## Test Topology

```
                    +-------+
    +---------------|  PC   |----------------+
    |               +-------+                |
    | ADB/SSH                                | ADB/SSH/SERIAL
    |                                        |
+-------+                           +------------------+
|  DUT  |<-----------Thread-------->| Reference Device |
+-------+                           +------------------+

```

- PC : The computer to run the test script.
- DUT : The device under test.
- Reference Device : The device that supports all tested features.

### Python Dependences

Before running the test script on PC, testers should install dependences first.

```bash
$ pip3 install -r ./tools/cp-caps/requirements.txt ./tools/otci
```

### Reference Device

The [nRF52840DK][ot-nrf528xx-nrf52840] is set as the reference device by default. Testers can also select the other Thread device as the reference device.

[ot-nrf528xx-nrf52840]: https://github.com/openthread/ot-nrf528xx/blob/main/src/nrf52840/README.md

Quick guide to setting up the nRF52840DK:

```bash
$ git clone git@github.com:openthread/ot-nrf528xx.git
$ cd ot-nrf528xx/
$ git submodule update --init
$ ./script/bootstrap
$ ./script/build nrf52840 UART_trans -DOT_DIAGNOSTIC=ON -DOT_CSL_RECEIVER=ON -DOT_LINK_METRICS_INITIATOR=ON -DOT_LINK_METRICS_SUBJECT=ON -DOT_WAKEUP_END_DEVICE=ON
$ arm-none-eabi-objcopy -O ihex build/bin/ot-cli-ftd ot-cli-ftd.hex
$ nrfjprog -f nrf52 --chiperase --program ot-cli-ftd.hex --reset
```

## Test Commands

### Help

Show help info.

```bash
$ python3 ./tools/cp-caps/rcp_caps_test.py -h
usage: rcp_caps_test.py [-h] [-v] [-D] [--test-all] [--test-csl] [--test-data-poll] [--test-diag-commands] [--test-frame-formats] [--test-link-metrics] [--test-tx-info] [--test-throughput]

This script is used for testing RCP capabilities.

options:
  -h, --help            show this help message and exit
  -v, --version         output version
  -D, --debug           output debug information
  --test-all            test all test cases
  --test-csl            test whether the RCP supports CSL transmitter
  --test-data-poll      test whether the RCP supports data poll
  --test-diag-commands  test whether the RCP supports all diag commands
  --test-frame-formats  test whether the RCP supports 802.15.4 frames of all formats
  --test-link-metrics   test whether the RCP supports link metrics
  --test-tx-info        test mTxInfo field of the radio frame
  --test-throughput     test Thread network 1-hop throughput

Device Interfaces:
  DUT_ADB_TCP=<device_ip>        Connect to the DUT via adb tcp
  DUT_ADB_USB=<serial_number>    Connect to the DUT via adb usb
  DUT_CLI_SERIAL=<serial_device> Connect to the DUT via cli serial port
  DUT_SSH=<device_ip>            Connect to the DUT via ssh
  REF_ADB_USB=<serial_number>    Connect to the reference device via adb usb
  REF_CLI_SERIAL=<serial_device> Connect to the reference device via cli serial port
  REF_SSH=<device_ip>            Connect to the reference device via ssh
  ADB_KEY=<adb_key>              Full path to the adb key

Examples:
Run test cases of the specified categories:
  DUT_ADB_USB=1169UC2F2T0M95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-diag-commands --test-data-poll

Run specified test case:
  DUT_ADB_USB=1169UC2F2T0M95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 -m unittest -q tools.cp-caps.rcp_caps_test.TestDiagCommands.test_diag_channel
```

> Note: If you meet the error `LIBUSB_ERROR_BUSY` when you are using the ADB usb interface, please run the command `adb kill-server` to kill the adb server.

### Test Diag Commands

The option `--test-diag-commands` tests all diag commands.

Following environment variables are used to configure diag command parameters:

- DUT_DIAG_GPIO: Diag gpio value. The default value is `0` if it is not set.
- DUT_DIAG_RAW_POWER_SETTING: Diag raw power setting value. The default value is `112233` if it is not set.
- DUT_DIAG_POWER: Diag power value. The default value is `10` if it is not set.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_CLI_SERIAL=/dev/ttyACM0 DUT_DIAG_GPIO=2 DUT_DIAG_RAW_POWER_SETTING=44556688 DUT_DIAG_POWER=11 python3 ./tools/cp-caps/rcp_caps_test.py --test-diag-commands
diag channel --------------------------------------------- OK
diag channel 20 ------------------------------------------ OK
diag cw start -------------------------------------------- OK
diag cw stop --------------------------------------------- OK
diag echo 0123456789 ------------------------------------- OK
diag echo -n 10 ------------------------------------------ OK
diag frame 00010203040506070809 -------------------------- OK
diag gpio mode 2 ----------------------------------------- OK
diag gpio mode 2 in -------------------------------------- OK
diag gpio mode 2 out ------------------------------------- OK
diag gpio get 2 ------------------------------------------ OK
diag gpio set 2 0 ---------------------------------------- OK
diag gpio set 2 1 ---------------------------------------- OK
diag power ----------------------------------------------- OK
diag power 11 -------------------------------------------- OK
diag powersettings --------------------------------------- NotSupported
diag powersettings 20 ------------------------------------ NotSupported
diag radio receive --------------------------------------- OK
diag radio sleep ----------------------------------------- OK
diag radio state ----------------------------------------- OK
diag rawpowersetting enable ------------------------------ NotSupported
diag rawpowersetting 44556688 ---------------------------- NotSupported
diag rawpowersetting ------------------------------------- NotSupported
diag rawpowersetting disable ----------------------------- NotSupported
diag repeat 10 64 ---------------------------------------- OK
diag repeat stop ----------------------------------------- OK
diag send 100 64 ----------------------------------------- OK
diag stats ----------------------------------------------- OK
diag stats clear ----------------------------------------- OK
diag stream start ---------------------------------------- NotSupported
diag stream stop ----------------------------------------- NotSupported
```

### Test CSL Transmitter

The option `--test-csl` tests whether the RCP supports the CSL transmitter.

```bash
$ DUT_ADB_USB=TW69UCKFZTGM95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-csl
CSL Transmitter ------------------------------------------ OK
```

### Test Data Poll

The option `--test-data-poll` tests whether the RCP supports data poll.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-data-poll
Data Poll Child ------------------------------------------ OK
Data Poll Parent ----------------------------------------- OK
```

### Test Link Metrics

The option `--test-link-metrics` tests whether the RCP supports link metrics.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-link-metrics
Link Metrics Initiator ----------------------------------- OK
Link Metrics Subject ------------------------------------- OK
```

### Test Throughput

The option `--test-throughput` tests the Thread network 1-hop throughput of the DUT.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_ADB_USB=44061HFAG01AQK python3 ./tools/cp-caps/rcp_caps_test.py --test-throughput
Throughput ----------------------------------------------- 75.6 Kbits/sec
```

### Test Frame Format

The option `--test-frame-formats` tests whether the RCP supports sending and receiving 802.15.4 frames of all formats.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-frame-formats
TX ver:2003,Cmd,seq,dst[addr:short,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2003,Cmd,seq,dst[addr:short,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2003,Bcon,seq,dst[addr:no,pan:no],src[addr:extd,pan:id],sec:no,ie:no,plen:30 ---------------- OK
RX ver:2003,Bcon,seq,dst[addr:no,pan:no],src[addr:extd,pan:id],sec:no,ie:no,plen:30 ---------------- OK
TX ver:2003,MP,noseq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:l5,ie[ren con],plen:0 --------- OK
RX ver:2003,MP,noseq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:l5,ie[ren con],plen:0 --------- OK
TX ver:2006,Cmd,seq,dst[addr:short,pan:id],src[addr:short,pan:no],sec:l5,ie:no,plen:0 -------------- OK
RX ver:2006,Cmd,seq,dst[addr:short,pan:id],src[addr:short,pan:no],sec:l5,ie:no,plen:0 -------------- OK
TX ver:2006,Cmd,seq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:l5,ie:no,plen:0 ---------------- OK
RX ver:2006,Cmd,seq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:l5,ie:no,plen:0 ---------------- OK
TX ver:2006,Data,seq,dst[addr:extd,pan:id],src[addr:extd,pan:id],sec:no,ie:no,plen:0 --------------- OK
RX ver:2006,Data,seq,dst[addr:extd,pan:id],src[addr:extd,pan:id],sec:no,ie:no,plen:0 --------------- OK
TX ver:2006,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ------------- OK
RX ver:2006,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ------------- OK
TX ver:2006,Data,seq,dst[addr:extd,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2006,Data,seq,dst[addr:extd,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2006,Data,seq,dst[addr:short,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ---------------- OK
RX ver:2006,Data,seq,dst[addr:short,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ---------------- OK
TX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:no,pan:no],sec:no,ie:no,plen:0 ------------------- OK
RX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:no,pan:no],sec:no,ie:no,plen:0 ------------------- OK
TX ver:2015,Data,seq,dst[addr:no,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ------------------- OK
RX ver:2015,Data,seq,dst[addr:no,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ------------------- OK
TX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2015,Data,seq,dst[addr:extd,pan:no],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2015,Data,seq,dst[addr:extd,pan:no],src[addr:no,pan:no],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:extd,pan:id],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:extd,pan:id],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:extd,pan:no],sec:no,ie:no,plen:0 ----------------- OK
RX ver:2015,Data,seq,dst[addr:no,pan:no],src[addr:extd,pan:no],sec:no,ie:no,plen:0 ----------------- OK
TX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:no,ie:no,plen:0 --------------- OK
RX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:extd,pan:no],sec:no,ie:no,plen:0 --------------- OK
TX ver:2015,Data,seq,dst[addr:extd,pan:no],src[addr:extd,pan:no],sec:no,ie:no,plen:0 --------------- OK
RX ver:2015,Data,seq,dst[addr:extd,pan:no],src[addr:extd,pan:no],sec:no,ie:no,plen:0 --------------- OK
TX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ------------- OK
RX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ------------- OK
TX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:extd,pan:id],sec:no,ie:no,plen:0 -------------- OK
RX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:extd,pan:id],sec:no,ie:no,plen:0 -------------- OK
TX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 -------------- OK
RX ver:2015,Data,seq,dst[addr:extd,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 -------------- OK
TX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie[csl],plen:0 ----------- OK
RX ver:2015,Data,seq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie[csl],plen:0 ----------- OK
TX ver:2015,Data,noseq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ----------- OK
RX ver:2015,Data,noseq,dst[addr:short,pan:id],src[addr:short,pan:id],sec:no,ie:no,plen:0 ----------- OK
```

### Test mTxInfo field of the radio frame.

The option `--test-tx-info` tests whether the RCP supports the mTxInfo field of the radio frame.

```bash
$ DUT_ADB_USB=1269UCKFZTAM95OR REF_CLI_SERIAL=/dev/ttyACM0 python3 ./tools/cp-caps/rcp_caps_test.py --test-tx-info
mCsmaCaEnabled=0 ----------------------------------------- OK
mCsmaCaEnabled=1 ----------------------------------------- OK
mIsSecurityProcessed=True -------------------------------- OK
mIsSecurityProcessed=False ------------------------------- OK
mMaxCsmaBackoffs=0 --------------------------------------- OK (12 ms)
mMaxCsmaBackoffs=100 ------------------------------------- OK (541 ms)
mRxChannelAfterTxDone ------------------------------------ OK
mTxDelayBaseTime=now,mTxDelay=500000 --------------------- OK
```
