
# TSAM Assignment 8 – The Botnet Saves the World

## Description

This project is a TCP messaging system written in C++.

It builds on Assignment 6 by allowing servers to connect and communicate with each other, instead of only handling clients.

Servers can receive messages from clients, forward them to other servers, and store them if the destination is currently unavailable.

This allows messages to be passed between different groups, even when the receiving server is not directly connected.

## Group Information

- **Group ID:** `G_uinness`
- **Course:** T-409-TSAM – Computer Networks
- **Assignment:** 8 – The Botnet Saves the World
- **Language:** C++
- **Platform:** Linux


## Files

- `tsamserver.cpp` – TCP server implementation
- `tsamclient.cpp` – TCP client implementation
- `identity.h` – Contains the group's identity information
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

The server responds with:

```text
Welcome
```

### SENDMSG

Sends a message to another user or group.

```text
SENDMSG,<from>,<to>,<message>
```



The server adds a timestamp before forwarding the message to another server.

### GETMSG

Retrieves the oldest unread message from a specified sender.

```text
GETMSG,<from>
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


If there are no unread messages, the server responds with `EMPTY`.

## Server-to-Server Commands

### HELO

Exchanges group identities when servers connect.

```text
HELO,<group-id>
```


### SERVERS

Shares information about known servers.

```text
SERVERS,<group-id>,<ip>,<port>;...
```

The received information is used for discovering additional servers and determining possible forwarding routes.

### KEEPALIVE

Notifies a connected server about the number of messages waiting for it.

```text
KEEPALIVE,<count>
```

`

The receiving server can request its waiting messages using `GETMSGS`.

KEEPALIVE messages are sent periodically, no more than once per minute.

### GETMSGS

Requests stored messages for a specified group.

```text
GETMSGS,<group-id>
```


### SENDMSG

Transfers a message between servers.

```text
SENDMSG,<from>,<to>,<timestamp>,<message>
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




## Bonus Points

Our group worked on the following two bonus tasks:

### Early Bot (10 points)

We completed an early version of the server-to-server communication within the first week.

Our server was able to connect to another server, exchange `HELO` and `SERVERS` commands, and send messages between servers.

The implementation and logs of the exchange were submitted for the Early Bot bonus.

### Indirect Bot (20 points)

Our server also supports indirect message delivery, meaning messages can travel through other servers instead of going directly from the sender to the recipient.

This bonus consists of two parts:

- **Relaying (10 points):** Forwarding messages between other groups, where our server is neither the original sender nor the final recipient.
- **Patience (10 points):** Storing messages when the destination server is unreachable and delivering them when a connection becomes available.

#### Indirect Bot Timestamps

The following timestamps from November 2, 2026, can be used to locate the relevant messages in `server.log`:

```text
02/11/2026 19:44:42
02/11/2026 22:42:48
02/11/2026 22:25:59
02/11/2026 22:23:56
02/11/2026 22:23:56
02/11/2026 22:23:56
02/11/2026 22:23:56
02/11/2026 21:01:47
02/11/2026 21:01:47
02/11/2026 20:56:26
```

These timestamps help identify the message exchanges used for the Indirect Bot bonus.

The corresponding log entries should show the incoming and outgoing `SENDMSG` commands for relaying, and message storage followed by successful delivery for patience.



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


## Course

Reykjavik University  
T-409-TSAM – Computer Networks  
Assignment 8 – The Botnet Saves the World
