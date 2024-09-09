# Xmodem Server - README

## Project Overview

This project implements an **xmodem server** that receives and stores files sent by an xmodem client. The server and client communicate over an unreliable connection, where the server ensures reliable file transfer using the **CRC16** algorithm to detect data corruption and request retransmission if necessary.

## Xmodem Protocol Overview

The xmodem protocol transfers files in blocks, ensuring data integrity over unreliable connections:
- **Block Structure (132 bytes)**:
  - 1 byte: SOH (start of header)
  - 1 byte: Block number
  - 1 byte: Inverse of block number (255 - block number)
  - 128 bytes: File payload (padded with SUB if necessary)
  - 2 bytes: CRC16 of the payload

## CRC16

The **CRC16** algorithm calculates a 16-bit checksum to detect transmission errors. The client sends each block with its CRC16 checksum, and the server verifies it upon receipt.

## Server State Machine

The server operates in the following states:

1. **Initial**:  
   - Receives filename from the client and opens it for writing.
   - Sends a `C` to signal readiness for receiving blocks.

2. **Pre-block**:  
   - Waits for either SOH (128-byte block), STX (1024-byte block), or EOT (end of transfer).

3. **Get Block**:  
   - Reads the block (132 or 1028 bytes) sent by the client.

4. **Check Block**:  
   - Verifies the block number, inverse, and CRC16.
   - Handles errors: sends ACK for success, NAK for retransmission, or aborts transfer on serious errors.

5. **Finish**:  
   - Cleans up after transfer completion.

## File Handling

- Files are stored in a directory named **`filestore/`**.
- Only files with a `.junk` extension are accepted for transfer to prevent accidental overwriting of important files.
  
## Client-Server Interaction

1. **Client sends filename**:  
   Server opens the file for writing.
   
2. **Handshake**:  
   Client waits for a `C` from the server.

3. **Block transmission**:  
   Client sends blocks with SOH/STX, block number, inverse, payload, and CRC16.

4. **Server verifies each block**:  
   Sends ACK if successful, NAK if there are errors.

5. **End of transfer**:  
   Client sends EOT, server sends final ACK.

## Error Handling

- The server checks the block number, inverse, and CRC16 for errors.
- On error, the server sends a NAK to request retransmission.
- Duplicate or out-of-sequence blocks result in the client being dropped.