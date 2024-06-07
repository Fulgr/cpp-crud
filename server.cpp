#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8001"
#define BUFFER_SIZE 1024

struct Message {
    int id;
    std::string client_id;
    std::string text;
};

std::unordered_map<int, Message> messages;
int current_id = 1;
std::mutex messages_mutex;

void handle_client(SOCKET client_socket);
std::vector<std::string> split(const std::string& str, char delimiter);
void send_response(SOCKET client_socket, const std::string& response);

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << DEFAULT_PORT << std::endl;

    SOCKET client_socket;
    while (true) {
        client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }

        std::thread(handle_client, client_socket).detach();
    }

    closesocket(listen_socket);
    WSACleanup();
    return 0;
}

void handle_client(SOCKET client_socket) {
    char recvbuf[BUFFER_SIZE];
    int iResult;

    do {
        iResult = recv(client_socket, recvbuf, BUFFER_SIZE, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::string request(recvbuf);
            std::vector<std::string> parts = split(request, ' ');

            if (parts.size() < 2) {
                send_response(client_socket, "ERROR: Invalid request format.\n");
                continue;
            }

            std::string command = parts[0];
            std::string client_id = parts[1];

            if (command == "POST") {
                if (parts.size() < 3) {
                    send_response(client_socket, "ERROR: Missing message text.\n");
                    continue;
                }
                std::string message_text = request.substr(command.size() + client_id.size() + 2);

                std::lock_guard<std::mutex> lock(messages_mutex);
                Message new_message = { current_id++, client_id, message_text };
                messages[new_message.id] = new_message;
                send_response(client_socket, "Message posted with ID: " + std::to_string(new_message.id) + "\n");
            }
            else if (command == "READ") {
                std::stringstream response;
                std::lock_guard<std::mutex> lock(messages_mutex);
                for (const auto& entry : messages) {
                    response << "ID: " << entry.second.id << ", Client ID: " << entry.second.client_id << ", Message: " << entry.second.text << "\n";
                }
                send_response(client_socket, response.str());
            }
            else if (command == "UPDATE") {
                if (parts.size() < 4) {
                    send_response(client_socket, "ERROR: Missing message ID or text.\n");
                    continue;
                }
                int message_id = std::stoi(parts[2]);
                std::string new_text = request.substr(command.size() + client_id.size() + parts[2].size() + 3);

                std::lock_guard<std::mutex> lock(messages_mutex);
                if (messages.find(message_id) != messages.end() && messages[message_id].client_id == client_id) {
                    messages[message_id].text = new_text;
                    send_response(client_socket, "Message updated.\n");
                }
                else {
                    send_response(client_socket, "ERROR: Message not found or unauthorized.\n");
                }
            }
            else if (command == "DELETE") {
                if (parts.size() < 3) {
                    send_response(client_socket, "ERROR: Missing message ID.\n");
                    continue;
                }
                int message_id = std::stoi(parts[2]);

                std::lock_guard<std::mutex> lock(messages_mutex);
                if (messages.find(message_id) != messages.end() && messages[message_id].client_id == client_id) {
                    messages.erase(message_id);
                    send_response(client_socket, "Message deleted.\n");
                }
                else {
                    send_response(client_socket, "ERROR: Message not found or unauthorized.\n");
                }
            }
            else {
                send_response(client_socket, "ERROR: Unknown command.\n");
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closing...\n";
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return;
        }
    } while (iResult > 0);

    iResult = shutdown(client_socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "shutdown failed: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    closesocket(client_socket);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void send_response(SOCKET client_socket, const std::string& response) {
    send(client_socket, response.c_str(), response.size(), 0);
}
