# Build

```bash
$ ./build.sh
```

# Run test

```bash
$ expect ./tests/scripts/expect/cli-peer-to-peer.exp
```


# Log

Check log on Linux:

```bash
$ tail -f  /var/log/syslog | grep ot-cli > log.txt
$ sed -i -r 's/.{66}//' file ./log.txt
```
