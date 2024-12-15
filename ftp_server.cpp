#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <mutex>
#include <algorithm>
#include <iomanip>

#define PORT 2121
#define BUFFER_SIZE 1024

namespace fs = std::filesystem;

void handle_client(int client_socket);
void send_response(int client_socket, const std::string& response);
std::string receive_command(int client_socket);
bool validate_input(const std::string& input);
void handle_data_connection(int client_socket, const std::string& command, int data_port, bool is_passive, const std::string& client_ip, int passive_socket, const std::string& user_directory);
void handle_list_command(int data_socket, const std::string& user_directory, int client_socket);
void handle_retr_command(int data_socket, const std::string& user_directory, const std::string& filename, int client_socket);
void handle_stor_command(int data_socket, const std::string& user_directory, const std::string& filename, int client_socket);
void handle_help_command(int client_socket, const std::string& command);
void set_data_port(const std::string& port_command, int &data_port, std::string &client_ip);
void enable_passive_mode(int client_socket, int  &passive_socket, int &data_port);
bool validate_username(const std::string& username);
bool validate_password(const std::string& username, const std::string& password);
std::string hash_password(const std::string& password);

std::string current_type = "A";
int default_data_port = PORT - 1;
std::mutex client_mutex;

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error: Unable to create socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error: Unable to bind socket");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        perror("Error: Unable to listen on socket");
        close(server_socket);
        return 1;
    }

    std::cout << "FTP Server started on port " << PORT << "." << std::endl;

    while (true) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Error: Unable to accept client connection");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        std::cout << "Client connected: " << client_ip << std::endl;
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    close(server_socket);
    return 0;
}

std::string hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    size_t hashed = hasher(password);
    std::ostringstream hash_stream;
    hash_stream << std::hex << hashed;
    return hash_stream.str();
}

bool validate_username(const std::string& username) {
    std::ifstream file("credentials.txt");
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open credentials file" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        std::string stored_username;
        line_stream >> stored_username;
        if (stored_username == username) {
            return true;
        }
    }

    return false;
}

bool validate_password(const std::string& username, const std::string& password) {
    std::ifstream file("credentials.txt");
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open credentials file" << std::endl;
        return false;
    }

    std::string line;
    std::string hashed_password = hash_password(password);

    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        std::string stored_username, stored_hash;
        line_stream >> stored_username >> stored_hash;

        if (stored_username == username && stored_hash == hashed_password) {
            return true;
        }
    }

    return false;
}

void handle_client(int client_socket) {
    int data_port = default_data_port;
    int passive_socket = -1;
    bool is_passive = false;
    std::string client_ip = "";
    std::string current_username;
    std::string user_directory;
    bool is_authenticated = false;

    try {
        send_response(client_socket, "220 Welcome to the FTP server\r\n");

        while (true) {
            std::string command = receive_command(client_socket);
            if (command.empty()) {
                std::cout << "Client disconnected." << std::endl;
                close(client_socket);
                if (passive_socket != -1) {
                    close(passive_socket);
                }
                break;
            }

            std::cout << "Received command: " << command << std::endl;

            if (!validate_input(command)) {
                send_response(client_socket, "500 Invalid command syntax.\r\n");
                continue;
            }

            if (command.substr(0, 4) == "USER") {
                current_username = command.substr(5);
                if (validate_username(current_username)) {
                    send_response(client_socket, "331 Username OK, need password.\r\n");
                } else {
                    send_response(client_socket, "530 Invalid username.\r\n");
                    current_username.clear();
                }
            }
            else if (command.substr(0, 4) == "PASS") {
                if (current_username.empty()) {
                    send_response(client_socket, "503 Login with USER first.\r\n");
                    continue;
                }

                std::string provided_password = command.substr(5);
                if (validate_password(current_username, provided_password)) {
                    is_authenticated = true;
                    user_directory = "ftp_root/" + current_username;
                    send_response(client_socket, "230 Login successful.\r\n");
                } else {
                    send_response(client_socket, "530 Invalid password.\r\n");
                    current_username.clear();
                }
            }
            else if (command.substr(0, 4) == "QUIT") {
                send_response(client_socket, "221 Goodbye.\r\n");
                close(client_socket);
                if (passive_socket != -1) {
                    close(passive_socket);
                }
                break;
            }
            else if (command.substr(0, 4) == "HELP") {
                handle_help_command(client_socket, command.substr(5));
            }
            else if (!is_authenticated) {
                send_response(client_socket, "530 Not logged in.\r\n");
            }
            else if (command.substr(0, 4) == "TYPE") {
                std::string type = command.substr(5);
                if (type == "A") {
                    current_type = "A";
                    send_response(client_socket, "200 Type set to ASCII.\r\n");
                } else if (type == "I") {
                    current_type = "I";
                    send_response(client_socket, "200 Type set to Binary.\r\n");
                } else {
                    send_response(client_socket, "504 Command not implemented for that parameter.\r\n");
                }
            }
            else if (command.substr(0, 4) == "PORT") {
                set_data_port(command, data_port, client_ip);
                is_passive = false;
                send_response(client_socket, "200 Data port set for active mode.\r\n");
            }
            else if (command.substr(0, 4) == "PASV") {
                enable_passive_mode(client_socket, passive_socket, data_port);
                is_passive = true;
            }
            else if (command.substr(0, 4) == "LIST") {
                handle_data_connection(client_socket, "LIST", data_port, is_passive, client_ip, passive_socket, user_directory);
            }
            else if (command.substr(0, 4) == "RETR") {
                handle_data_connection(client_socket, command, data_port, is_passive, client_ip, passive_socket, user_directory);
            }
            else if (command.substr(0, 4) == "STOR") {
                handle_data_connection(client_socket, command, data_port, is_passive, client_ip, passive_socket, user_directory);
            }
            else {
                send_response(client_socket, "502 Command not implemented.\r\n");
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling client: " << e.what() << std::endl;
        close(client_socket);
        if (passive_socket != -1) {
            close(passive_socket);
        }
    }
}

void handle_help_command(int client_socket, const std::string& command) {
    if (command.empty()) {
        send_response(client_socket, "214 Supported commands: USER, PASS, TYPE, PORT, PASV, LIST, RETR, STOR, HELP, QUIT\r\n");
    } else {
        if (command == "USER") {
            send_response(client_socket, "214 USER: Specify username to login.\r\n");
        } else if (command == "PASS") {
            send_response(client_socket, "214 PASS: Specify password after USER.\r\n");
        } else if (command == "TYPE") {
            send_response(client_socket, "214 TYPE: Set transfer mode (A for ASCII, I for Binary).\r\n");
        } else if (command == "PORT") {
            send_response(client_socket, "214 PORT: Specify client data port.\r\n");
        } else if (command == "PASV") {
            send_response(client_socket, "214 PASV: Enter passive mode for data transfer.\r\n");
        } else if (command == "LIST") {
            send_response(client_socket, "214 LIST: List directory contents.\r\n");
        } else if (command == "RETR") {
            send_response(client_socket, "214 RETR: Retrieve file from server.\r\n");
        } else if (command == "STOR") {
            send_response(client_socket, "214 STOR: Store file on server.\r\n");
        } else if (command == "QUIT") {
            send_response(client_socket, "214 QUIT: Close the connection.\r\n");
        } else {
            send_response(client_socket, "504 Unknown command in HELP.\r\n");
        }
    }
}

void enable_passive_mode(int client_socket, int &passive_socket, int &data_port) {
    try {
        if (passive_socket != -1) {
            close(passive_socket);
        }

        passive_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (passive_socket == -1) {
            perror("Error: Unable to create passive socket");
            send_response(client_socket, "425 Cannot open passive connection.\r\n");
            return;
        }

        struct sockaddr_in passive_addr;
        passive_addr.sin_family = AF_INET;
        passive_addr.sin_addr.s_addr = INADDR_ANY;
        passive_addr.sin_port = htons(0);

        if (bind(passive_socket, (struct sockaddr*)&passive_addr, sizeof(passive_addr)) == -1) {
            perror("Error: Unable to bind passive socket");
            send_response(client_socket, "425 Cannot open passive connection.\r\n");
            close(passive_socket);
            passive_socket = -1;
            return;
        }

        socklen_t passive_len = sizeof(passive_addr);
        if (getsockname(passive_socket, (struct sockaddr*)&passive_addr, &passive_len) == -1) {
            perror("Error: Unable to get passive socket name");
            send_response(client_socket, "425 Cannot open passive connection.\r\n");
            close(passive_socket);
            passive_socket = -1;
            return;
        }

        data_port = ntohs(passive_addr.sin_port);

        if (listen(passive_socket, 1) == -1) {
            perror("Error: Unable to listen on passive socket");
            send_response(client_socket, "425 Cannot open passive connection.\r\n");
            close(passive_socket);
            passive_socket = -1;
            return;
        }

        char server_ip[INET_ADDRSTRLEN];
        struct sockaddr_in server_addr;
        socklen_t server_len = sizeof(server_addr);
        if (getsockname(client_socket, (struct sockaddr*)&server_addr, &server_len) == 0) {
            inet_ntop(AF_INET, &server_addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        } else {
            strcpy(server_ip, "127.0.0.1");
        }

        std::replace(server_ip, server_ip + strlen(server_ip), '.', ',');
        send_response(client_socket, "227 Entering Passive Mode (" + std::string(server_ip) + "," + std::to_string(data_port / 256) + "," + std::to_string(data_port % 256) + ").\r\n");
    } catch (const std::exception& e) {
        std::cerr << "Error enabling passive mode: " << e.what() << std::endl;
        if (passive_socket != -1) {
            close(passive_socket);
        }
        passive_socket = -1;
        send_response(client_socket, "425 Cannot open passive connection.\r\n");
    }
}

void handle_data_connection(int client_socket, const std::string& command, int data_port, bool is_passive, const std::string& client_ip, int passive_socket, const std::string& user_directory) {
    try {
        int data_socket;
        if (is_passive) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            data_socket = accept(passive_socket, (struct sockaddr*)&client_addr, &client_len);
            if (data_socket == -1) {
                perror("Error: Unable to accept data connection");
                send_response(client_socket, "425 Cannot open passive data connection.\r\n");
                return;
            }
        } else {
            data_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (data_socket == -1) {
                perror("Error: Unable to create active data socket");
                send_response(client_socket, "425 Cannot open active data connection.\r\n");
                return;
            }

            struct sockaddr_in client_addr;
            client_addr.sin_family = AF_INET;
            inet_pton(AF_INET, client_ip.c_str(), &client_addr.sin_addr);
            client_addr.sin_port = htons(data_port);

            if (connect(data_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
                perror("Error: Unable to connect to data socket");
                send_response(client_socket, "425 Cannot open active data connection.\r\n");
                close(data_socket);
                return;
            }
        }
        
        if (command == "LIST") {
            handle_list_command(data_socket, user_directory, client_socket);
        } else if (command.substr(0, 4) == "RETR") {
            handle_retr_command(data_socket, user_directory, command.substr(5), client_socket);
        } else if (command.substr(0, 4) == "STOR") {
            handle_stor_command(data_socket, user_directory, command.substr(5), client_socket);
        }

        close(data_socket);
    } catch (const std::exception& e) {
        std::cerr << "Error handling data connection: " << e.what() << std::endl;
        send_response(client_socket, "451 Requested action aborted.\r\n");
    }
}

void handle_list_command(int data_socket, const std::string& user_directory, int client_socket) {
    try {
        std::ostringstream list;
        for (const auto& entry : fs::directory_iterator(user_directory)) {
            list << entry.path().filename().string() << "\r\n";
        }
        std::string response = list.str();
        send(data_socket, response.c_str(), response.size(), 0);
        send_response(client_socket, "226 Directory send OK.\r\n");
    } catch (const std::exception& e) {
        send_response(client_socket, "451 Requested action aborted: Failed to list directory.\r\n");
        std::cerr << "Error during LIST: " << e.what() << std::endl;
    }
}

void handle_retr_command(int data_socket, const std::string& user_directory, const std::string& filename, int client_socket) {
    try {
        std::string filepath = user_directory + "/" + filename;
        std::ifstream file(filepath, current_type == "A" ? std::ios::in : std::ios::binary);
        if (!file) {
            send_response(client_socket, "550 File not found.\r\n");
            return;
        }

        send_response(client_socket, "150 Opening data connection.\r\n");

        char buffer[BUFFER_SIZE];
        if (current_type == "A") {
            std::string line;
            while (std::getline(file, line)) {
                line += "\r\n";
                send(data_socket, line.c_str(), line.size(), 0);
            }
        } else {
            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                send(data_socket, buffer, file.gcount(), 0);
            }
        }

        send_response(client_socket, "226 Transfer complete.\r\n");
    } catch (const std::exception& e) {
        send_response(client_socket, "451 Requested action aborted: Failed to retrieve file.\r\n");
        std::cerr << "Error during RETR: " << e.what() << std::endl;
    }
}

void handle_stor_command(int data_socket, const std::string& user_directory, const std::string& filename, int client_socket) {
    try {
        std::string filepath = user_directory + "/" + filename;

        std::ofstream file(filepath, current_type == "A" ? std::ios::out : std::ios::binary);
        if (!file.is_open()) {
            send_response(client_socket, "550 Cannot create file.\r\n");
            return;
        }

        send_response(client_socket, "150 Opening data connection.\r\n");

        char buffer[BUFFER_SIZE];
        int bytes_read;

        while ((bytes_read = recv(data_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            file.write(buffer, bytes_read); 
        }

        if (bytes_read < 0) {
            perror("Error receiving data");
            send_response(client_socket, "426 Connection closed; transfer aborted.\r\n");
        } else {
            send_response(client_socket, "226 Transfer complete.\r\n");
        }

        file.close();
    } catch (const std::exception& e) {
        send_response(client_socket, "451 Requested action aborted: Failed to store file.\r\n");
        std::cerr << "Error during STOR: " << e.what() << std::endl;
    }
}



void set_data_port(const std::string& port_command, int &data_port, std::string &client_ip) {
    try {
        size_t start = port_command.find(' ') + 1;
        std::string params = port_command.substr(start);

        std::replace(params.begin(), params.end(), ',', '.');

        std::stringstream ss(params);
        int h1, h2, h3, h4, p1, p2;
        char dot;
        ss >> h1 >> dot >> h2 >> dot >> h3 >> dot >> h4 >> dot >> p1 >> dot >> p2;

        if (ss.fail() || dot != '.') {
            throw std::invalid_argument("Invalid PORT command format");
        }

        client_ip = std::to_string(h1) + "." + std::to_string(h2) + "." + std::to_string(h3) + "." + std::to_string(h4);
        data_port = (p1 << 8) + p2;

        if (data_port <= 0 || data_port > 65535) {
            throw std::out_of_range("Port number out of range");
        }

        std::cout << "Client IP: " << client_ip << " Data port set to " << data_port << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing PORT command: " << e.what() << std::endl;
        throw;
    }
}

void send_response(int client_socket, const std::string& response) {
    if (send(client_socket, response.c_str(), response.size(), 0) == -1) {
        perror("Error: Failed to send response");
    }
}

std::string receive_command(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cout << "Client closed the connection." << std::endl;
        } else {
            perror("Error: Failed to receive command");
        }
        return "";
    }
    std::string command = std::string(buffer, bytes_received);

    command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());
    command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());

    return command;
}

bool validate_input(const std::string& input) {
    if (input.size() > 512) {
        return false;
    }

    for (char c : input) {
        if (!isprint(c) && c != '\r' && c != '\n') {
            return false;
        }
    }

    return true;
}