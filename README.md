## Contributors

* Huarcaya Lizarraga, Astrid Judith
* Flores Choque, Anderzon Miguel
* Mellado Baca, Cristian
* Murillo Castillo, Alexander Rafael 
* Panuera Cruz, Miguel Angel 

# Communication Protocol Specification

This document defines the communication protocol for data between the central server and clients.

---

## Operation: Send Information

### Server &rarr; Clients (Response)
Allows the server to send information to clients.

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | Packet Type (1: packet) |
| **Number** | 4 Bytes | The packet sequence number |
| **Size** | 4 Bytes | Total number of characters in the checksum |
| **Information**| 1 Byte | Checksum |
| **Number** | 4 Bytes | Total number of packets |
| **Number** | 4 Bytes | Packet number |
| **Number** | 2 Bytes | Number layer |
| **Size** | 4 Bytes | Size of the data to be sent |
| **Information**| Variable| Data to be sent |


### Client &rarr; Server (Request)
Allows the client to send information to server. 

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | Packet Type (1: packet, 0: Register) |
| **Number** | 4 Bytes | The packet sequence number |
| **Size** | 4 Bytes | Total number of characters in the checksum |
| **Information**| 1 Byte | Checksum |
| **Number** | 4 Bytes | Total number of packets |
| **Number** | 4 Bytes | Packet number |
| **Number** | 2 Bytes | Number layer |
| **Size** | 4 Bytes | Size of the data to be sent |
| **Information**| Variable| Data to be sent |

---

## Operation: ACK
Acknowledges the successful receipt of a packet.

**Direction:** Client &rarr; Server (Response)

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | Packet Type (2: ACK) |
| **Number** | 4 Bytes | The packet sequence number |

---

## Operation: NACK
Signals a negative acknowledgment, indicating a corrupted or missing packet.

**Direction:** Client &rarr; Server (Response)

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | Packet Type (3: NACK) |
| **Number** | 4 Bytes | The packet sequence number |

---

## Usage Examples

### Register Client
```text
0
0
```
### Send Information Packets
```text
100010001CHECKSUM00050001010004HOLA
100020001CHECKSUM00050002010005PABLO
100030001CHECKSUM00050003010004HOLA
100040001CHECKSUM00050004010004Juan
100050001CHECKSUM00050005010011Informacion
```

### ACK
```text
20001
20002
```

### NACK
```text
30001
30002
```