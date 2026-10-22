
# TSAM Assignment 7 - TCP Mail Server and Client

## Description

This project implements a TCP mail server and client in C++.

The server supports multiple clients using `poll()`. Clients can send messages to other users, retrieve unread messages, and request the number of stored messages.

Messages are stored on the server until they are retrieved.

## Project Files

| File | Description |
|------|-------------|
| `tsamserver.cpp` | TCP server implementation |
| `tsamclient.cpp` | TCP client implementation |
| `Makefile` | Compiles the server and client |
| `server.log` | Server activity log |
| `client-output.txt` | Recorded client output |
| `README.md` | Project documentation |

## Compilation

Compile both programs using:

```bash
make
```

Alternatively, compile them manually:

```bash
g++ tsamserver.cpp -o tsamserver
g++ tsamclient.cpp -o tsamclient
```

## Running the Programs

**1. Start the server**

Open a terminal and run:

```bash
./tsamserver <port>
```

**2. Start the client**

Open another terminal and run:

```bash
./tsamclient <ip> <port>
```

The IP address `127.0.0.1` connects to a server running on the same computer. Port `4044` is an example and can be changed.

After connecting, the client asks for a username. Once the username is sent, the client can enter commands.

Multiple clients can connect to the server at the same time.

## Supported Commands

### SENDMSG

Send a message to another user.

**Format:**

```text
SENDMSG,<from>,<to>,<message>
```

**Example:**

```text
SENDMSG,Alice,Bob,Hello Bob
```

The server stores the message for Bob until it is retrieved.

### GETMSG

Retrieve an unread message for a user.

**Format:**

```text
GETMSG,<username>
```

**Example:**

```text
GETMSG,Bob
```

If no unread messages are available, the server returns `EMPTY`.

### STATUSREQ

Request the number of unread messages stored for each user.

**Format:**

```text
STATUSREQ
```

**Example response:**

```text
Alice:2,Bob:1
```

If no unread messages are stored, the server returns `EMPTY`.

## TCP Message Format

All commands and responses use the following frame structure:

```text
SOH | Length (2 bytes) | STX | Message | ETX
```

| Field | Description |
|-------|-------------|
| SOH | Start of header (`0x01`) |
| Length | Total frame length in network byte order |
| STX | Start of text (`0x02`) |
| Message | Command or response |
| ETX | End of text (`0x03`) |

The maximum frame size is **5000 bytes**, including the header and control bytes.


## Logging

The server records activity in `server.log`

### Saving Client Output

To display client output and save it to `client-output.txt` at the same time, run:

```bash
./tsamclient 127.0.0.1 4044 | tee client-output.txt
```

The client displays timestamps for commands and server responses.

## Closing the Programs

To disconnect the client, enter:

```text
Q
```

To stop the server, press `Ctrl+C` in the server terminal.

## Implementation

- **Language:** C++
- **Transport protocol:** TCP
- **Multiple clients:** `poll()`
- **Message framing:** SOH, length, STX, payload, ETX
- **Platform:** Linux / Ubuntu (WSL)
