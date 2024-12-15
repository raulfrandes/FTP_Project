#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdlib>

#define BUFFER_SIZE 1024

void send_command(int socket, const std::string& command);
std::string receive_response(int socket);
int setup_active_mode(int control_socket, int &data_socket);
int setup_passive_mode(int control_socket, int &data_socket);
void handle_list(int control_socket, int data_socket);
void handle_retr(int control_socket, int data_socket, const std::string& filename);
void handle_stor(int control_socket, int data_socket, const std::string& filename);

bool is_passive_mode = false; // Tracks current mode

int main() {
    std::string server_ip = "127.0.0.1";
    int server_port = 2121;

    int control_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (control_socket == -1) {
        perror("Error: Unable to create control socket");
        return 1;
    }

    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    if (connect(control_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error: Unable to connect to FTP server");
        close(control_socket);
        return 1;
    }

    std::cout << "Connected to FTP server at " << server_ip << ":" << server_port << "\n";

    std::string response = receive_response(control_socket);
    std::cout << "Server: " << response;

    bool is_logged_in = false;

    while (true) {
        std::cout << "ftp> ";
        std::string command;
        std::getline(std::cin, command);

        if (command.empty()) {
            continue;
        }

        std::string cmd = command.substr(0, 4);

        if (cmd == "QUIT") {
            send_command(control_socket, command);
            std::cout << receive_response(control_socket);
            break;
        }

        if (cmd == "USER" || cmd == "PASS") {
            send_command(control_socket, command);
            response = receive_response(control_socket);
            std::cout << "Server: " << response;
            if (response.find("230") != std::string::npos) {
                is_logged_in = true;
            }
        } else if (!is_logged_in) {
            std::cout << "Please log in first.\n";
        } else if (cmd == "TYPE" || cmd == "HELP") {
            send_command(control_socket, command);
            std::cout << "Server: " << receive_response(control_socket);
        } else if (cmd == "PORT") {
            int data_socket = -1;
            if (setup_active_mode(control_socket, data_socket) != -1) {
                is_passive_mode = false;
                std::cout << "Active mode set.\n";
                close(data_socket); // Only set up mode, no actual data transfer here
            }
        } else if (cmd == "PASV") {
            int data_socket = -1;
            if (setup_passive_mode(control_socket, data_socket) != -1) {
                is_passive_mode = true;
                std::cout << "Passive mode set.\n";
                close(data_socket); // Only set up mode, no actual data transfer here
            }
        } else if (cmd == "LIST") {
            int data_socket = -1;
            if ((is_passive_mode && setup_passive_mode(control_socket, data_socket) != -1) ||
                (!is_passive_mode && setup_active_mode(control_socket, data_socket) != -1)) {
                send_command(control_socket, command);
                handle_list(control_socket, data_socket);
                close(data_socket);
            }
        } else if (cmd == "RETR") {
            int data_socket = -1;
            if ((is_passive_mode && setup_passive_mode(control_socket, data_socket) != -1) ||
                (!is_passive_mode && setup_active_mode(control_socket, data_socket) != -1)) {
                send_command(control_socket, command);
                handle_retr(control_socket, data_socket, command.substr(5));
                close(data_socket);
            }
        } else if (cmd == "STOR") {
            int data_socket = -1;
            if ((is_passive_mode && setup_passive_mode(control_socket, data_socket) != -1) ||
                (!is_passive_mode && setup_active_mode(control_socket, data_socket) != -1)) {
                send_command(control_socket, command);
                handle_stor(control_socket, data_socket, command.substr(5));
                close(data_socket);
            }
        } else {
            send_command(control_socket, command);
            std::cout << "Server: " << receive_response(control_socket);
        }
    }

    close(control_socket);
    return 0;
}

void send_command(int socket, const std::string& command) {
    std::string cmd = command + "\r\n";
    if (send(socket, cmd.c_str(), cmd.size(), 0) == -1) {
        perror("Error: Failed to send command");
    }
}

std::string receive_response(int socket) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cout << "Server closed the connection.\n";
        } else {
            perror("Error: Failed to receive response");
        }
        return "";
    }
    return std::string(buffer, bytes_received);
}

int setup_active_mode(int control_socket, int &data_socket) {
    data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data_socket == -1) {
        perror("Error: Unable to create data socket");
        return -1;
    }

    struct sockaddr_in client_addr {};
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(0);

    if (bind(data_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
        perror("Error: Unable to bind data socket");
        close(data_socket);
        return -1;
    }

    if (listen(data_socket, 1) == -1) {
        perror("Error: Unable to listen on data socket");
        close(data_socket);
        return -1;
    }

    struct sockaddr_in addr {};
    socklen_t addr_len = sizeof(addr);
    getsockname(data_socket, (struct sockaddr*)&addr, &addr_len);

    uint16_t port = ntohs(addr.sin_port);
    std::string ip = "127,0,0,1";
    std::string port_command = "PORT " + ip + "," + std::to_string(port / 256) + "," + std::to_string(port % 256);

    send_command(control_socket, port_command);
    std::cout << "Server: " << receive_response(control_socket);

    return 0;
}

int setup_passive_mode(int control_socket, int &data_socket) {
    send_command(control_socket, "PASV");
    std::string response = receive_response(control_socket);

    if (response.find("227") == std::string::npos) {
        std::cerr << "Error: Failed to enter passive mode.\n";
        return -1;
    }

    size_t start = response.find('(') + 1;
    size_t end = response.find(')');
    std::string pasv_info = response.substr(start, end - start);

    std::replace(pasv_info.begin(), pasv_info.end(), ',', '.');
    std::istringstream pasv_stream(pasv_info);

    int h1, h2, h3, h4, p1, p2;
    char dot;
    pasv_stream >> h1 >> dot >> h2 >> dot >> h3 >> dot >> h4 >> dot >> p1 >> dot >> p2;

    std::string ip = std::to_string(h1) + "." + std::to_string(h2) + "." + std::to_string(h3) + "." + std::to_string(h4);
    int port = (p1 << 8) + p2;

    data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data_socket == -1) {
        perror("Error: Unable to create data socket");
        return -1;
    }

    struct sockaddr_in data_addr {};
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &data_addr.sin_addr);

    if (connect(data_socket, (struct sockaddr*)&data_addr, sizeof(data_addr)) == -1) {
        perror("Error: Unable to connect to passive mode data socket");
        close(data_socket);
        return -1;
    }

    return 0;
}

void handle_list(int control_socket, int data_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received;
    while ((bytes_received = recv(data_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::cout << std::string(buffer, bytes_received);
    }
    std::cout << receive_response(control_socket);
}

void handle_retr(int control_socket, int data_socket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Unable to create file.\n";
        return;
    }

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received;
    while ((bytes_received = recv(data_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytes_received);
    }
    file.close();

    std::cout << receive_response(control_socket);
    std::cout << receive_response(control_socket);
}

void handle_stor(int control_socket, int data_socket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for upload.\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        if (send(data_socket, buffer, file.gcount(), 0) == -1) {
            perror("Error: Failed to send data");
            break;
        }
    }

    file.close();

    close(data_socket);

    std::cout << receive_response(control_socket);
    std::cout << receive_response(control_socket);
}

