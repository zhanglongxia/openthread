# Run Demo
To try it on the simulation, you may download the OpenThread code and build with ./build.sh. Then there are 3 scripts to play with:

```bash
./tests/scripts/expect/cli-wake-2.exp
```

This involves 2 sleepy nodes, both detached. The first node wakes up the second, then the two nodes establishe a secured link-local CSL link. The second one registers its service to the first one.

```bash
./tests/scripts/expect/cli-wake-3.exp
```

This one is similar to cli-wake-2, but with 3 nodes. Node 1 link Node 2, Node 2 link Node 3 and Node 3 link Node 1.


```bash
./tests/scripts/expect/cli-wake.exp
```

This one is more complex. There's a leader, two attached SSEDs, and two detached sleepy nodes. WED1 is linked to both SSED1 and SSED2, SSED2 is linked to WED1 and WED2.

# What's included in the current draft?

- establishing the secured Peer-Peer link with unicast Link Request with 3 messages(Link Request, Link Accept and Request, Link Accept),
- registering DNS service to Wake Coordinator,
- maintaining links with multiple peers,
- simultaneously keeping a Parent-Child link together with multiple Peer-Peer links. 

Note that the last Link Accept message is used to exchange CSL parameters. There may be some security issues to be considered.

# What's remaining?

- exchanging Busy Intervals and CSL Sample Window between CSL peers,
- adding random delay to CSL transmission if the peer shared CSL Sample Window,
- adding SRP Proxy to Wake Coordinator who is in the attached state,
- etc..
