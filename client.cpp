#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8001"
#define BUFFER_SIZE 1024

void menu();
void send_request(SOCKET client_socket, const std::string& request);
std::string receive_response(SOCKET client_socket);

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET connect_socket = INVALID_SOCKET;
    ptr = result;

    connect_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (connect_socket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = connect(connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(connect_socket);
        connect_socket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (connect_socket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server. You can now perform operations.\n";

    menu();

    char choice;
    std::string client_id;
    int message_id;
    std::string new_text;
    std::string message_text;

    while (true) {
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
        case 'P':
        case 'p':
            std::cout << "Enter your client ID: ";
            std::getline(std::cin, client_id);
            std::cout << "Enter message text: ";
            std::getline(std::cin, message_text);
            send_request(connect_socket, "POST " + client_id + " " + message_text);
            std::cout << receive_response(connect_socket);
            break;
        case 'R':
        case 'r':
            send_request(connect_socket, "READ");
            std::cout << receive_response(connect_socket);
            break;
        case 'U':
        case 'u':
            std::cout << "Enter your client ID: ";
            std::getline(std::cin, client_id);
            std::cout << "Enter message ID to update: ";
            std::cin >> message_id;
            std::cin.ignore();
            std::cout << "Enter new message text: ";
            std::getline(std::cin, new_text);
            send_request(connect_socket, "UPDATE " + client_id + " " + std::to_string(message_id) + " " + new_text);
            std::cout << receive_response(connect_socket);
            break;
        case 'D':
        case 'd':
            std::cout << "Enter your client ID: ";
            std::getline(std::cin, client_id);
            std::cout << "Enter message ID to delete: ";
            std::cin >> message_id;
            send_request(connect_socket, "DELETE " + client_id + " " + std::to_string(message_id));
            std::cout << receive_response(connect_socket);
            break;
        case 'Q':
        case 'q':
            std::cout << "Exiting...";
            closesocket(connect_socket);
            WSACleanup();
            return 0;
        default:
            std::cerr << "Invalid choice!" << std::endl;
            break;
        }
    }

    return 0;
}

void menu() {
    std::cout << "Menu:\n";
    std::cout << "P: Post a message\n";
    std::cout << "R: Read messages\n";
    std::cout << "U: Update a message\n";
    std::cout << "D: Delete a message\n";
    std::cout << "Q: Quit\n";
}

void send_request(SOCKET client_socket, const std::string& request) {
    send(client_socket, request.c_str(), request.size(), 0);
}

std::string receive_response(SOCKET client_socket) {
    char recvbuf[BUFFER_SIZE];
    int iResult = recv(client_socket, recvbuf, BUFFER_SIZE, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';
        return std::string(recvbuf);
    }
    else {
        return "recv failed: " + std::to_string(WSAGetLastError());
    }
}
