# UDP Forwarder

UDP Forwarder is a small program to forward `UDP` packets around.

This is useful to forward `UDP` packets in `NAT`-ed environments where you can't port-forward properly or you don't have access to the router configuration.

This has been built for a very specific use-case however you might be able to use it as well.

## Compilation

There aren't pre-built binaries. You will need a C17 compiler to build it.

There is a makefile, you can `make server` and `make client` to build respectively the server and the client binaries.

### Debug mode

You can compile in debug mode by using `make DEBUG=1 server` or `make DEBUG=1 client`. This will enable additional logging to help debugging issues.

## Usage

Client usage:
```
./client <server ip> <server port> <forward ip> <forward port> <password>
```

Server usage:
```
./server <incoming port> <forward port> <password>
```

### Server

The server will listen to both ports, when it receives a message on the `incoming port` it will send a packet to the `forward port` with the content of the received message, and vice-versa.

Until a [client](#client) advertizes itself with the password the server will drop all packets. 

### Client

The client will connect to the server at `<server ip>:<server port>` and advertize itself by sending the `<password>` string.

Then when the client receives packets from the server with the source ip and port for the client it will open a connection to `<forward ip>:<forward port>` and forward the packets there, and vice-versa.

This allows to have multiple clients connecting to the same server and forwarding packets to different destinations.

## Why not using `socat`

`socat` is a very powerful tool however I have found it to not being able to do what UDP Forwarder does, especially for the client binary. For this reason this tool exists.

## Example use-case (Factorio)

[Factorio](https://factorio.com) uses `UDP` for its multiplayer server, however when the server is in a `NAT`-ed environment and you want to play with friend that is an issue.

Tradizionally you would use something like a VPS and an `SSH` tunnel to expose the local port and that works great. However, `SSH` does not support `UDP`, only `TCP`
and factorio doesn't seem to enjoing having its packet being converted from `UDP` to `TCP` and then from `TCP` to `UDP` again.

To solve this you can run UDP Forwarder server on the VPS, with a cmdline such as: `./server <port from where you connect> <some port>`.
After you run the command UDP Forwarder will listen to those 2 ports as explained in the server explaination above.

Then on the same server that is running the Factorio server you can run UDP Forwarder client, with a cmdline such as: `./client <vps ip> <vps port> 127.0.0.1 <factorio port>`.
After that the client will connect to the UDP Forwarder server running on the VPS and advertize itself, as described in the client explaination above.

After both server and client are running, when a Factorio client opens a connection with the VPS on the port you decided
the UDP Forwarder server on the VPS will send the packets to the UDP Forwarder client running on the server running the Factorio server and then it will send the data to Factorio to handle the connection.

Since this opens a new socket for each different ip + port combination that the server receives, you can have multiple players with a single client and server.

## License

The code is publish under the MIT licence.
