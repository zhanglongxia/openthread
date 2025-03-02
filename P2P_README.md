# Guide

This project implements a simple peer-to-peer demo.

# Demo

Demo topology:

```
 WC1 (SSED) <----\
                  + --> WED (SSED)
 WC2 (SSED) <----/
```

Demo steps:

1. Setup WED

2. Setup WC1

3. WC1 establishs a P2P link with WED and enables the SRP server

4. WED pings WC1 and starts the SRP client

5. WC1 checks all registered srp server hosts

6. Setup WC2

7. WED enables the wake-up listener

8. WC2 establishs a P2P link with WED and enables the SRP server

9. WED pings WC2 and starts the SRP client

10. WC2 checks all registered srp server hosts

11. WED tears down p2p link from WC1 and WC2

# Get P2P Demo Code

```bash
$ git clone git@github.com:openthread/openthread.git
$ cd openthread
$ git remote add upstream git@github.com:zhanglongxia/openthread.git
$ git fetch upstream p2p-demo
$ git checkout -b p2p-demo upstream/p2p-demo
```

# Simulation

Run the P2P demo on simulation nodes.

## Build

```bash
$ ./build.sh
```

## Run demo

Run the peer-to-peer demo on the simulation nodes.

```bash
$ expect ./tests/scripts/expect/cli-peer-to-peer.exp
```

## Log

Check log on Linux:

```bash
$ tail -f  /var/log/syslog | grep ot-cli > log.txt
```

# nRF52840DK

Run the P2P demo on three nRF52840DKs.

## Prerequisites

1. Prepare three nRF52840DKs and connect then to the PC.

2. Check that there are three serial ports.

```bash
$ ls /dev/ | grep ACM
ttyACM0
ttyACM1
ttyACM2
```

3. Get the serial number of three nRF52840DKs.

```bash
$ nrfjprog -i
683688378
683152435
68312907
```

4. Get ot-nrf528xx source code.

```bash
$ git clone git@github.com:openthread/ot-nrf528xx.git
$ cd ot-nrf528xx
$ git remote add upstream git@github.com:zhanglongxia/ot-nrf528xx.git
$ git fetch upstream p2p-demo
$ git checkout -b p2p-demo upstream/p2p-demo
```

5. Update the serial number of the script `nrf.sh`.

```bash
$ cd ot-nrf528xx
$ vim ./nrf.sh
SERIAL_NUMBER_1=683688378
SERIAL_NUMBER_2=683152435
SERIAL_NUMBER_3=683129075
```

## Build and Flash images

Link the OpenThread source code:

```bash
$ cd ot-nrf528xx
$ rm ./openthread
$ ln -s <path-to-openthread> ./openthread
```

Build the fimrware:

```bash
$ cd ot-nrf528xx
$ nrf.sh build
```

Flash the fimrware into three nRF52840DKs:

```bash
$ cd ot-nrf528xx
$ nrf.sh flash
```

## Run demo

Run the peer-to-peer demo on three nRF52840DKs.

```bash
$ OT_NODE_TYPE=serial expect ./tests/scripts/expect/cli-peer-to-peer.exp
```
