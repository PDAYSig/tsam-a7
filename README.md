
# TSAM Assignment 8 – The Botnet Saves the World

## Description

This project implements a distributed TCP messaging system in C++.

It extends the client-server messaging system from Assignment 6 by allowing multiple servers to connect and exchange messages.

Each server can receive messages from clients, forward messages to other servers, and store messages when their destination is not reachable.

The system uses a store-and-forward approach to deliver messages between different groups.

## Group Information

- **Group ID:** `G_uinness`
- **Course:** T-409-TSAM – Computer Networks
- **Assignment:** 8 – The Botnet Saves the World
- **Language:** C++
- **Platform:** Linux

## Files

- `tsamserver.cpp` – TCP server implementation
- `tsamclient.cpp` – TCP client implementation
- `Makefile` – Compiles the project
- `server.log` – Records server activity
- `README.md` – Project documentation

## Compilation

Compile the project using:

```bash
make
```

This builds the executables defined in the Makefile.

## Running the Server

Start the server by specifying a listening port:

```bash
./tsamserver 4000
```

The server listens for incoming TCP connections on the specified port.

The same listening socket accepts connections from both clients and other servers.

On startup, the server attempts to connect to the bootstrap server:

- **IP:** `130.208.246.98`
- **Port:** `5001`

The bootstrap server helps discover other servers in the network.

## Running the Client

Start the client using:

```bash
./tsamclient <server-ip> <server-port>
```

For example:

```bash
./tsamclient 127.0.0.1 4000
```

The client connects to the specified server and can send messages, retrieve messages, and request message status.

## Communication Protocol

All communication uses TCP.

Messages are transmitted using the following frame structure:

```text
<SOH><LENGTH><STX><COMMAND><ETX>
```

| Field | Description |
|-------|-------------|
| SOH | Start of header (0x01) |
| LENGTH | 2-byte big-endian total frame length |
| STX | Start of text (0x02) |
| COMMAND | Message command and parameters |
| ETX | End of text (0x03) |

The maximum supported frame size is 5000 bytes.

The server maintains a receive buffer for each socket to handle incomplete TCP frames and multiple frames arriving together.

## Client Commands

### USERNAME

Registers a client username.

```text
USERNAME,<username>
```

Example:

```text
USERNAME,olgeir
```

The server responds with:

```text
Welcome
```

### SENDMSG

Sends a message to another user or group.

```text
SENDMSG,<from>,<to>,<message>
```

Example:

```text
SENDMSG,olgeir,G_goats,Hello!
```

The server adds a timestamp before forwarding the message to another server.

### GETMSG

Retrieves the oldest unread message from a specified sender.

```text
GETMSG,<from>
```

Example:

```text
GETMSG,G_goats
```

If no matching message exists, the server responds with:

```text
EMPTY
```

### STATUSREQ

Requests information about unread messages.

```text
STATUSREQ
```

Example response:

```text
G_goats:2,G_nammi:1
```

This indicates two messages from `G_goats` and one from `G_nammi`.

If there are no unread messages, the server responds with `EMPTY`.

## Server-to-Server Commands

### HELO

Exchanges group identities when servers connect.

```text
HELO,<group-id>
```

Example:

```text
HELO,G_uinness
```

### SERVERS

Shares information about known servers.

```text
SERVERS,<group-id>,<ip>,<port>;...
```

Example:

```text
SERVERS,G_uinness,130.208.246.98,4000;G_goats,130.208.246.98,4022
```

The received information is used for discovering additional servers and determining possible forwarding routes.

### KEEPALIVE

Notifies a connected server about the number of messages waiting for it.

```text
KEEPALIVE,<count>
```

Example:

```text
KEEPALIVE,3
```

The receiving server can request its waiting messages using `GETMSGS`.

KEEPALIVE messages are sent periodically, no more than once per minute.

### GETMSGS

Requests stored messages for a specified group.

```text
GETMSGS,<group-id>
```

Example:

```text
GETMSGS,G_uinness
```

### SENDMSG

Transfers a message between servers.

```text
SENDMSG,<from>,<to>,<timestamp>,<message>
```

Example:

```text
SENDMSG,olgeir,G_goats,1793500000,Hello!
```

The original timestamp is preserved when the message is forwarded.

### STATUSREQ

Requests information about messages stored on another server.

```text
STATUSREQ
```

### STATUSRESP

Returns the number of stored messages for each destination group.

```text
STATUSRESP,<group-id>:<count>,...
```

Example:

```text
STATUSRESP,G_goats:2,G_nammi:1
```

If no messages are stored:

```text
STATUSRESP,EMPTY
```

## Message Storage and Forwarding

Messages are stored in memory using:

```cpp
map<string, vector<unread_message>> UNREAD_MESSAGES;
```

Each message contains:

- Message content
- Sender
- Destination
- Timestamp

When a message arrives, the server checks its destination.

1. If the message is addressed to the local server, it is stored for client retrieval.
2. If the destination is a directly connected server, the message is forwarded.
3. If a connected server reports a route to the destination, the message can be forwarded through that server.
4. If no route is known, the message is stored until delivery becomes possible.

The server uses information received through `SERVERS`, `KEEPALIVE`, and `STATUSRESP` to support message delivery.

## Connection Management

The server uses `poll()` to monitor multiple TCP sockets without creating a separate thread for each connection.

It maintains:

- `CONNECTED_SERVERS` – Currently connected peer servers
- `FRIEND_SERVERS` – Discovered servers available for connection
- `clients` – Connected clients
- `fds` – File descriptors monitored by `poll()`

Incoming connections are identified using their initial commands:

- `HELO` identifies another server.
- `USERNAME` identifies a client.

Disconnected sockets are removed from the relevant connection structures.

## Logging

Server activity is written to:

```text
server.log
```

The log includes events such as:

- Server startup
- Incoming connections
- Successful and failed connection attempts
- Received protocol commands
- Message forwarding
- Message storage and delivery
- Socket disconnections

To view recent log entries:

```bash
tail -n 30 server.log
```

To monitor the log continuously:

```bash
tail -f server.log
```

To clear the log:

```bash
> server.log
```

## Implementation Details

The project uses:

- C++ standard library containers
- POSIX TCP sockets
- `poll()` for handling multiple connections
- Per-socket receive buffers
- Custom binary message framing
- In-memory message queues
- Server discovery and message forwarding
- Timestamp preservation for forwarded messages

No third-party networking libraries are required.

## Notes

Messages are currently stored in memory and are not preserved across server restarts.

The server is designed to run on Linux and communicate with other student servers using the Assignment 8 protocol.

## Course

Reykjavik University  
T-409-TSAM – Computer Networks  
Assignment 8 – The Botnet Saves the World
