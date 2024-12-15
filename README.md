# FTP Client-Server Project

This project implements the **File Transfer Protocol (FTP)** as described in [RFC 959](https://datatracker.ietf.org/doc/html/rfc959). It includes a client and a server application written in C++, providing core FTP functionalities such as file transfer, directory listing, login/logout, and support for both active and passive modes.

---

## Features

### General Features
- **Protocol Compliance**: Implements the FTP standard (RFC 959).
- **Client-Server Interaction**: The client and server can communicate with any standard-compliant FTP implementation.
- **Input Validation**: Robust validation of input parameters and commands.
- **Modes of Operation**:
  - Active mode (default).
  - Passive mode (switchable using the `PASV` command).

### Client Functionality
- **Connect/Disconnect**: Establish and terminate connections with the server.
- **Authentication**: User login and password authentication.
- **File Transfer**:
  - Upload (`STOR`) and download (`RETR`) files (supports binary and ASCII modes).
  - Directory listing (`LIST`) with detailed output.
- **Transfer Modes**:
  - ASCII (`TYPE A`).
  - Binary (`TYPE I`).
- **Command Support**: Includes `USER`, `PASS`, `PORT`, `PASV`, `LIST`, `RETR`, `STOR`, and `QUIT`.

### Server Functionality
- **User Management**:
  - Reads user credentials from `credentials.txt`.
  - Supports hashed passwords for secure authentication.
- **File Management**:
  - Maintains separate directories for authenticated users.
  - Handles file uploads and downloads with appropriate permissions.
- **Command Handling**: Processes all client commands (`LIST`, `STOR`, `RETR`, etc.) with detailed response codes.

---

## Project Requirements

### Client
- Implements the client-side of the FTP protocol.
- Connects to any standard FTP server and performs all functionalities as per the standard.

### Server
- Implements the server-side of the FTP protocol.
- Accepts connections and commands from any standard FTP client.

### Supported Commands
- **USER**: Provide a username for login.
- **PASS**: Authenticate using a password.
- **PORT**: Enable active mode and set the client data port.
- **PASV**: Enable passive mode for data transfer.
- **LIST**: List directory contents.
- **RETR**: Retrieve a file from the server.
- **STOR**: Upload a file to the server.
- **TYPE**: Set the transfer mode (ASCII or binary).
- **QUIT**: Disconnect from the server.

---

## Setup and Compilation

### Prerequisites
- C++ compiler with support for C++17 or later.
- POSIX-compatible environment (Linux recommended).

### Compilation
1. Clone the repository:
   ```bash
   git clone https://github.com/your-repo/ftp-project.git
   cd ftp-project
