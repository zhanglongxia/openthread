# OpenThread Diagnostics - Sitesurvey Example

## Quick Start

### Node 1

Set the device as a sitesurvey server.

```bash
> diag sitesurvey listen
 282.306: State: Disabled -> HandshakeListening
Listening channel 12 on 8e72826f9ed1b68f
Done
```

### Node 2

Set the device as a sitesurvey client. The client will connect to the server first, then the client/server will start to send 15.4 frames based on the configuration. After all 15.4 frames are sent out, the client/server will send the test result to the client.

```bash
> diag channel 12
set channel to 12
status 0x00
Done
> diag sitesurvey send dstaddr 8e72826f9ed1b68f direction tx channel 20 attempts 20 length 64 number 100 interval 50
 415.106: State: Disabled -> HandshakeSending
Client connecting to 8e72826f9ed1b68f, channel 12
Done
>  415.146: [SEQ=0] Send Handshake Request
 415.187: [SEQ=1] Send Handshake Request
 415.207: [SEQ=1][2] Received Handshake ACK (65535)
 415.207: [SEQ=1] Send Handshake Ack
 415.208: State: HandshakeSending -> HandshakeEstablished
 415.208: Delay(700) Delta=20 415.208: Connected with 8e72826f9ed1b68f
 415.237: [SEQ=2][2] Received Handshake ACK (65535)
 415.238: [SEQ=2] Send Handshake Ack
 415.908: State: HandshakeEstablished -> SendingData
 415.948: TX, seq=0, ch=20, len=64
 416.000: TX, seq=1, ch=20, len=64
 416.051: TX, seq=2, ch=20, len=64
 416.101: TX, seq=3, ch=20, len=64
 416.152: TX, seq=4, ch=20, len=64
 416.204: TX, seq=5, ch=20, len=64
 416.255: TX, seq=6, ch=20, len=64
 416.306: TX, seq=7, ch=20, len=64
 416.357: TX, seq=8, ch=20, len=64
 416.408: TX, seq=9, ch=20, len=64
 416.459: TX, seq=10, ch=20, len=64
 416.510: TX, seq=11, ch=20, len=64
 416.561: TX, seq=12, ch=20, len=64
 416.612: TX, seq=13, ch=20, len=64
 416.662: TX, seq=14, ch=20, len=64
 416.714: TX, seq=15, ch=20, len=64
 416.766: TX, seq=16, ch=20, len=64
 416.816: TX, seq=17, ch=20, len=64
 416.866: TX, seq=18, ch=20, len=64
 416.916: TX, seq=19, ch=20, len=64
 416.966: TX, seq=20, ch=20, len=64
 417.017: TX, seq=21, ch=20, len=64
 417.068: TX, seq=22, ch=20, len=64
 417.119: TX, seq=23, ch=20, len=64
 417.172: TX, seq=24, ch=20, len=64
 417.222: TX, seq=25, ch=20, len=64
 417.273: TX, seq=26, ch=20, len=64
 417.324: TX, seq=27, ch=20, len=64
 417.375: TX, seq=28, ch=20, len=64
 417.441: TX, seq=29, ch=20, len=64
 417.491: TX, seq=30, ch=20, len=64
 417.543: TX, seq=31, ch=20, len=64
 417.593: TX, seq=32, ch=20, len=64
 417.644: TX, seq=33, ch=20, len=64
 417.695: TX, seq=34, ch=20, len=64
 417.745: TX, seq=35, ch=20, len=64
 417.795: TX, seq=36, ch=20, len=64
 417.846: TX, seq=37, ch=20, len=64
 417.897: TX, seq=38, ch=20, len=64
 417.950: TX, seq=39, ch=20, len=64
 418.001: TX, seq=40, ch=20, len=64
 418.053: TX, seq=41, ch=20, len=64
 418.104: TX, seq=42, ch=20, len=64
 418.156: TX, seq=43, ch=20, len=64
 418.207: TX, seq=44, ch=20, len=64
 418.259: TX, seq=45, ch=20, len=64
 418.310: TX, seq=46, ch=20, len=64
 418.361: TX, seq=47, ch=20, len=64
 418.411: TX, seq=48, ch=20, len=64
 418.463: TX, seq=49, ch=20, len=64
 418.514: TX, seq=50, ch=20, len=64
 418.565: TX, seq=51, ch=20, len=64
 418.615: TX, seq=52, ch=20, len=64
 418.665: TX, seq=53, ch=20, len=64
 418.716: TX, seq=54, ch=20, len=64
 418.767: TX, seq=55, ch=20, len=64
 418.818: TX, seq=56, ch=20, len=64
 418.869: TX, seq=57, ch=20, len=64
 418.920: TX, seq=58, ch=20, len=64
 418.971: TX, seq=59, ch=20, len=64
 419.022: TX, seq=60, ch=20, len=64
 419.073: TX, seq=61, ch=20, len=64
 419.125: TX, seq=62, ch=20, len=64
 419.176: TX, seq=63, ch=20, len=64
 419.227: TX, seq=64, ch=20, len=64
 419.277: TX, seq=65, ch=20, len=64
 419.328: TX, seq=66, ch=20, len=64
 419.379: TX, seq=67, ch=20, len=64
 419.430: TX, seq=68, ch=20, len=64
 419.480: TX, seq=69, ch=20, len=64
 419.531: TX, seq=70, ch=20, len=64
 419.581: TX, seq=71, ch=20, len=64
 419.632: TX, seq=72, ch=20, len=64
 419.684: TX, seq=73, ch=20, len=64
 419.734: TX, seq=74, ch=20, len=64
 419.785: TX, seq=75, ch=20, len=64
 419.835: TX, seq=76, ch=20, len=64
 419.886: TX, seq=77, ch=20, len=64
 419.937: TX, seq=78, ch=20, len=64
 419.988: TX, seq=79, ch=20, len=64
 420.038: TX, seq=80, ch=20, len=64
 420.089: TX, seq=81, ch=20, len=64
 420.140: TX, seq=82, ch=20, len=64
 420.192: TX, seq=83, ch=20, len=64
 420.243: TX, seq=84, ch=20, len=64
 420.294: TX, seq=85, ch=20, len=64
 420.345: TX, seq=86, ch=20, len=64
 420.396: TX, seq=87, ch=20, len=64
 420.447: TX, seq=88, ch=20, len=64
 420.499: TX, seq=89, ch=20, len=64
 420.549: TX, seq=90, ch=20, len=64
 420.600: TX, seq=91, ch=20, len=64
 420.651: TX, seq=92, ch=20, len=64
 420.702: TX, seq=93, ch=20, len=64
 420.753: TX, seq=94, ch=20, len=64
 420.804: TX, seq=95, ch=20, len=64
 420.856: TX, seq=96, ch=20, len=64
 420.907: TX, seq=97, ch=20, len=64
 420.958: TX, seq=98, ch=20, len=64
 421.009: TX, seq=99, ch=20, len=64
 421.059: State: SendingData -> WaitingReport
 421.145: [SEQ=0][100] Received Report (7)
 421.145: WaitingReport: ReportSize=9, PayloadLen=7 421.145: [SEQ=0] Send Handshake Ack
 421.217: [SEQ=2][100] Received Report (7)
 421.217: WaitingReport: ReportSize=9, PayloadLen=7 421.217: [SEQ=2] Send Handshake Ack
 421.899: State: WaitingReport -> Disabled
 421.900: Report: Ch:20, Len:64, Sent:100, Received:93, MinRssi:-48, AvgRssi:-47, MaxRssi:-45, MinLqi:161, AvgLqi:191, MaxLqi:196
 421.900: Disconnected from 8e72826f9ed1b68f
```

### Node 1

The server shows the connection info and the frame info.

```bash
>  461.552: [SEQ=1][0] Received Handshake Request (6)
 461.553: [SEQ=1] Send Handshake Ack
 461.553: State: HandshakeListening -> HandshakeReceived
 461.593: [SEQ=2] Send Handshake Ack
 461.602: [SEQ=2][3] Received Handshake ACK (65535)
 461.602: State: HandshakeReceived -> HandshakeEstablished
 461.602: Delay(711) Delta=9 461.602: Connected with c26529cb5b5ebd33
 462.314: State: HandshakeEstablished -> ReceivingData
 462.416: RX, seq=2, ch=20, len=62, rssi=-45, lqi=196
 462.469: RX, seq=3, ch=20, len=62, rssi=-45, lqi=189
 462.517: RX, seq=4, ch=20, len=62, rssi=-45, lqi=189
 462.570: RX, seq=5, ch=20, len=62, rssi=-46, lqi=196
 462.620: RX, seq=6, ch=20, len=62, rssi=-46, lqi=175
 462.672: RX, seq=7, ch=20, len=62, rssi=-46, lqi=189
 462.723: RX, seq=8, ch=20, len=62, rssi=-46, lqi=196
 462.773: RX, seq=9, ch=20, len=62, rssi=-46, lqi=196
 462.825: RX, seq=10, ch=20, len=62, rssi=-46, lqi=189
 462.875: RX, seq=11, ch=20, len=62, rssi=-46, lqi=189
 462.926: RX, seq=12, ch=20, len=62, rssi=-46, lqi=189
 462.979: RX, seq=13, ch=20, len=62, rssi=-46, lqi=189
 463.029: RX, seq=14, ch=20, len=62, rssi=-47, lqi=189
 463.080: RX, seq=15, ch=20, len=62, rssi=-47, lqi=196
 463.133: RX, seq=16, ch=20, len=62, rssi=-47, lqi=189
 463.183: RX, seq=17, ch=20, len=62, rssi=-47, lqi=189
 463.232: RX, seq=18, ch=20, len=62, rssi=-47, lqi=189
 463.284: RX, seq=19, ch=20, len=62, rssi=-47, lqi=196
 463.332: RX, seq=20, ch=20, len=62, rssi=-47, lqi=196
 463.382: RX, seq=21, ch=20, len=62, rssi=-47, lqi=196
 463.434: RX, seq=22, ch=20, len=62, rssi=-47, lqi=189
 463.486: RX, seq=23, ch=20, len=62, rssi=-47, lqi=189
 463.536: RX, seq=24, ch=20, len=62, rssi=-47, lqi=196
 463.589: RX, seq=25, ch=20, len=62, rssi=-47, lqi=189
 463.637: RX, seq=26, ch=20, len=62, rssi=-47, lqi=189
 463.690: RX, seq=27, ch=20, len=62, rssi=-47, lqi=189
 463.742: RX, seq=28, ch=20, len=62, rssi=-47, lqi=196
 463.807: RX, seq=29, ch=20, len=62, rssi=-47, lqi=196
 463.857: RX, seq=30, ch=20, len=62, rssi=-47, lqi=189
 463.909: RX, seq=31, ch=20, len=62, rssi=-47, lqi=196
 463.960: RX, seq=32, ch=20, len=62, rssi=-47, lqi=182
 464.012: RX, seq=33, ch=20, len=62, rssi=-47, lqi=196
 464.061: RX, seq=34, ch=20, len=62, rssi=-47, lqi=189
 464.112: RX, seq=35, ch=20, len=62, rssi=-47, lqi=161
 464.161: RX, seq=36, ch=20, len=62, rssi=-47, lqi=189
 464.213: RX, seq=37, ch=20, len=62, rssi=-47, lqi=189
 464.263: RX, seq=38, ch=20, len=62, rssi=-48, lqi=189
 464.316: RX, seq=39, ch=20, len=62, rssi=-48, lqi=189
 464.367: RX, seq=40, ch=20, len=62, rssi=-48, lqi=196
 464.420: RX, seq=41, ch=20, len=62, rssi=-48, lqi=189
 464.471: RX, seq=42, ch=20, len=62, rssi=-48, lqi=182
 464.523: RX, seq=43, ch=20, len=62, rssi=-48, lqi=196
 464.574: RX, seq=44, ch=20, len=62, rssi=-48, lqi=189
 464.642: RX, seq=45, ch=20, len=62, rssi=-48, lqi=196
 464.675: RX, seq=46, ch=20, len=62, rssi=-48, lqi=189
 464.727: RX, seq=47, ch=20, len=62, rssi=-48, lqi=196
 464.778: RX, seq=48, ch=20, len=62, rssi=-48, lqi=196
 464.829: RX, seq=49, ch=20, len=62, rssi=-48, lqi=196
 464.881: RX, seq=50, ch=20, len=62, rssi=-48, lqi=189
 464.980: RX, seq=52, ch=20, len=62, rssi=-48, lqi=189
 465.032: RX, seq=53, ch=20, len=62, rssi=-48, lqi=189
 465.083: RX, seq=54, ch=20, len=62, rssi=-48, lqi=189
 465.133: RX, seq=55, ch=20, len=62, rssi=-48, lqi=196
 465.183: RX, seq=56, ch=20, len=62, rssi=-48, lqi=196
 465.235: RX, seq=57, ch=20, len=62, rssi=-48, lqi=189
 465.286: RX, seq=58, ch=20, len=62, rssi=-48, lqi=196
 465.338: RX, seq=59, ch=20, len=62, rssi=-48, lqi=196
 465.390: RX, seq=60, ch=20, len=62, rssi=-48, lqi=196
 465.441: RX, seq=61, ch=20, len=62, rssi=-48, lqi=196
 465.492: RX, seq=62, ch=20, len=62, rssi=-47, lqi=189
 465.541: RX, seq=63, ch=20, len=62, rssi=-47, lqi=196
 465.593: RX, seq=64, ch=20, len=62, rssi=-47, lqi=196
 465.644: RX, seq=65, ch=20, len=62, rssi=-47, lqi=189
 465.695: RX, seq=66, ch=20, len=62, rssi=-47, lqi=189
 465.747: RX, seq=67, ch=20, len=62, rssi=-47, lqi=196
 465.795: RX, seq=68, ch=20, len=62, rssi=-47, lqi=189
 465.846: RX, seq=69, ch=20, len=62, rssi=-47, lqi=196
 465.897: RX, seq=70, ch=20, len=62, rssi=-47, lqi=182
 465.948: RX, seq=71, ch=20, len=62, rssi=-47, lqi=189
 466.051: RX, seq=73, ch=20, len=62, rssi=-47, lqi=189
 466.100: RX, seq=74, ch=20, len=62, rssi=-47, lqi=196
 466.152: RX, seq=75, ch=20, len=62, rssi=-47, lqi=196
 466.254: RX, seq=77, ch=20, len=62, rssi=-47, lqi=189
 466.303: RX, seq=78, ch=20, len=62, rssi=-47, lqi=196
 466.355: RX, seq=79, ch=20, len=62, rssi=-47, lqi=189
 466.456: RX, seq=81, ch=20, len=62, rssi=-47, lqi=189
 466.505: RX, seq=82, ch=20, len=62, rssi=-47, lqi=196
 466.563: RX, seq=83, ch=20, len=62, rssi=-47, lqi=196
 466.610: RX, seq=84, ch=20, len=62, rssi=-47, lqi=189
 466.712: RX, seq=86, ch=20, len=62, rssi=-47, lqi=189
 466.763: RX, seq=87, ch=20, len=62, rssi=-47, lqi=189
 466.815: RX, seq=88, ch=20, len=62, rssi=-47, lqi=196
 466.865: RX, seq=89, ch=20, len=62, rssi=-47, lqi=189
 466.915: RX, seq=90, ch=20, len=62, rssi=-47, lqi=196
 466.967: RX, seq=91, ch=20, len=62, rssi=-48, lqi=196
 467.018: RX, seq=92, ch=20, len=62, rssi=-48, lqi=189
 467.068: RX, seq=93, ch=20, len=62, rssi=-48, lqi=189
 467.118: RX, seq=94, ch=20, len=62, rssi=-48, lqi=189
 467.170: RX, seq=95, ch=20, len=62, rssi=-48, lqi=189
 467.222: RX, seq=96, ch=20, len=62, rssi=-48, lqi=196
 467.275: RX, seq=97, ch=20, len=62, rssi=-48, lqi=189
 467.326: RX, seq=98, ch=20, len=62, rssi=-48, lqi=196
 467.376: RX, seq=99, ch=20, len=62, rssi=-48, lqi=196
 467.483: State: ReceivingData -> SendingReport
 467.490: (SEQ=0) Send Report
 467.530: (SEQ=1) Send Report
 467.571: (SEQ=2) Send Report
 467.582: [SEQ=2][3] Received Handshake ACK (65535)
 467.583: State: SendingReport -> HandshakeListening
 467.583: Report: Ch:20, Len:64, Sent:100, Received:93, MinRssi:-48, AvgRssi:-47, MaxRssi:-45, MinLqi:161, AvgLqi:191, MaxLqi:196
 467.583: Disconnected from c26529cb5b5ebd33
```


## Command List

- [disable](#disable)
- [listen](#listen)
- [send](#send)

## Command Details

### diag sitesurvey server start

Start the site survey server.

```bash
> diag sitesurvey server start
 282.306: State: Disabled -> HandshakeListening
Listening channel 12 on 8e72826f9ed1b68f
Done
```

### diag sitesurvey server stop

Stop the site survey server.

```bash
> diag sitesurvey server stop
Done
```

### diag sitesurvey client \[async\] \<extaddr\> \[-r\] \[-c channel\] \[-l length\] \[-n number\] \[-i interval\]

- async: Use the non-blocking mode.
- extaddr: The SiteSurvey server's extended MAC address.
- -r: The SiteSurvey server sends 15.4 frames to SiteSurvey client. If this parameter is not set, the SiteSurvey client sends 15.4 frames to the SiteSurvey server by default.
- -c: The channel to transmit and receive 15.4 frames. If this parameter is not set, the channel is set to same as the diag channel by default.
- -l: The length of the 15.4 frame. If this parameter is not set, the length is set to 64 by default.
- -n: The number of 15.4 frames transmitted. If this parameter is not set, the number is set to 100 by default.
- -i: The transmission interval of 15.4 frames, in unit of ms. If this parameter is not set, the interval is set to 10 ms by default.
