#!/usr/bin/expect -f

spawn ./build/simulation/examples/apps/cli/ot-cli-ftd  1
send "log level 5\r"
expect "Done"
send "panid 2\r"
expect "Done"
send "channel 25\r"
expect "Done"
send "ifconfig up\r"
expect "Done"
send "thread start\r"
expect "Done"
interact
