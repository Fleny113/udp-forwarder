# UDP Forwarder

UDP Forwarder is a small utility to forward `UDP` packets around. This has been built for a very specific use-case however you might be able to use it as well.

## Compilation

There aren't pre-built binaries. You will need to have [`.NET 8`](https://get.dot.net/8) installed to compile this.

## Usage

General command syntax:
```
./udp-forwarder <mode> <portA> <portB> [hostnameA] [hostnameB]
```


The tool is divided in 2 modes, mode 1 and mode 2.

### Mode 1

In this mode UDP Forwarder will listen to port A and port B and will redirect whatever it receives from a client on port A to port B and vice-versa.

Due to UDP limitations until both clients have sent a message the messages will have to be dropped.

In this mode the hostnames arguments are not used.

### Mode 2

In this mode UDP Forwarder will connect to `<hostname A>:<port A>` and to `<hostname B>:<port B>`, send a `X` character to `A` and then forward whatever it receives from A to B and vice-versa

The `X` message is these so that a UDP listener will have the remote address of the socket to comunitate.

This mode is meant to be used in combination to Mode 1.

## Why not using `socat`

`socat` is a very powerful tool however I have found it to not being able to do what UDP Forwarder does, especially for Mode 2. For this reason this tool exists.

## Example use-case (Factorio)

[Factorio](https://factorio.com) uses `UDP` for its multiplayer server, however when the server is in a `NAT`-ed environment and you want to play with friend that is an issue.

Tradizionally you would use something like a VPS and an `SSH` tunnel to expose the local port and that works great. However, `SSH` does not support `UDP`, only `TCP`
and factorio doesn't seem to enjoing having its packet being converted from `UDP` to `TCP` and then from `TCP` to `UDP` again.

To solve this you can run UDP Forwarder on the VPS in Mode 1, with a cmdline such as: `./udp-forwarder 1 <some port> <port from where you connect>`.
After you run the command UDP Forwarder will listen to those 2 ports as explained in the Mode 1 explaination above.

Then on the same server that is running the Factorio server you can run UDP Forwarder in Mode 2, with a cmdline such as: `./udp-forwarder 2 <vps port> <factorio port> <vps hostname> 127.0.0.1`.
After you run this UDP Forwarder will send a `X` to the UDP Forwarder running on the VPS so from there when a Factorio clients opens a connection with the VPS on the port you decided
the UDP Forwarder on the VPS will send the packets to the UDP Forwarder running on the server running the Factorio server and then it will send the data to Factorio to handle the connection.

### Limitations

Due to how this has been made, only 1 client can connect to the VPS port you are exposing, having more clients will require more UDP Forwarder instances running Mode 1 on the VPS and
Mode 2 instances running on the same server as the factorio server.


## License

The code is publish under the MIT licence.
