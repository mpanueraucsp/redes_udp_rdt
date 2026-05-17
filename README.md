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
| **Information** | Variable | Checksum information for the data to be sent |
| **Number** | 4 Bytes | Total number of packets |
| **Number** | 4 Bytes | Packet number |
| **Size** | 4 Bytes | Size of the data to be sent |
| **Information** | Variable | Data to be sent |


### Client &rarr; Server (Request)
Allows the client to send information to server. 

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | Packet Type (1: packet) |
| **Number** | 4 Bytes | The packet sequence number |
| **Size** | 4 Bytes | Total number of characters in the checksum |
| **Information** | Variable | Checksum information for the data to be sent |
| **Number** | 4 Bytes | Total number of packets |
| **Number** | 4 Bytes | Packet number |
| **Size** | 4 Bytes | Size of the data to be sent |
| **Information** | Variable | Data to be sent |

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

### Send Information Packets
```text
100010256CHECKSUM000500010004HOLA
100020256CHECKSUM000500020005PABLO
100030256CHECKSUM000500030004HOLA
100040256CHECKSUM000500040004Juan
100050256CHECKSUM000500050011Informacion
```

### Send Information Packets
```text
20001
20002
```

### Send Information Packets
```text
30001
30002
```