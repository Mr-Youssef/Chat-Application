#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <string>
#include <cstring>
#include <algorithm>
using namespace std;

const int PORT = 8080; // Port to listen on

struct Client {
    int socket;
    string username;
};

vector<Client> clients;
mutex clients_mutex;    // Mutex to protect clients vector
unordered_map<string, string> user_credentials;
vector<string> chat_history;
mutex chat_history_mutex;   // Mutex to protect chat_history vector

void save_message_to_history(const string& message) {
    lock_guard<mutex> lock(chat_history_mutex);     
    chat_history.push_back(message);
}

void send_chat_history(int client_socket) {
    lock_guard<mutex> history_lock(chat_history_mutex);
    
    string history_message = "                     --- Chat History ---\n";
    for (const auto& message : chat_history) {
        history_message += message + "\n";
    }
    history_message += "                --- End of Chat History ---\n";
    
    send(client_socket, history_message.c_str(), history_message.length(), 0);
}

void broadcast_message(const string& message, int sender_socket) {
    lock_guard<mutex> lock(clients_mutex);

    save_message_to_history(message);
    
    for (const auto& client : clients) {
        send(client.socket, message.c_str(), message.length(), 0);
    }
}

void send_connected_users_list() {
    lock_guard<mutex> lock(clients_mutex);

    string user_list = "Connected: " + to_string(clients.size()) + "\n";
    
    for (const auto& client : clients) {
        user_list += client.username + ",";
    }

    if (!user_list.empty()) {
        user_list.pop_back(); // Remove the last comma
    }

    for (const auto& client : clients) {
        send(client.socket, user_list.c_str(), user_list.length(), 0);
    }
}

bool authenticate_user(const string& username, const string& password) {
    auto it = user_credentials.find(username);
    if (it != user_credentials.end() && it->second == password) {
        return true;
    }
    return false;
}

void handle_client(int client_socket) {
    vector<char> buffer(1024, 0);
    string username, password;
    
    // Authentication
    int bytes_received = recv(client_socket, buffer.data(), buffer.size(), 0);
    if (bytes_received > 0) {
        string auth_data(buffer.begin(), buffer.begin() + bytes_received);
        size_t delimiter = auth_data.find(':'); // Delimiter between username and password
        if (delimiter != string::npos) {
            // Extract username and password from the received data
            username = auth_data.substr(0, delimiter);
            password = auth_data.substr(delimiter + 1);
        }
    }
    
    if (!authenticate_user(username, password)) {
        string error_msg = "Authentication failed";
        send(client_socket, error_msg.c_str(), error_msg.length(), 0);
        #ifdef _WIN32
        closesocket(client_socket);
        #else
        close(client_socket);
        #endif
        return;
    }
    
    string welcome_msg = "Welcome, " + username + "!\n";
    send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);

    // Send chat history to the client after successful authentication
    send_chat_history(client_socket);
    
    {
        lock_guard<mutex> lock(clients_mutex);
        clients.push_back({client_socket, username});
    }

    // Delay to ensure client processes chat history first, then the connected users list
    // without the delay, the client may receive the connected users list mixed with the chat history
    this_thread::sleep_for(chrono::milliseconds(100));

    // Send updated users list to all clients
    send_connected_users_list();

    while (true) {
        fill(buffer.begin(), buffer.end(), 0);
        bytes_received = recv(client_socket, buffer.data(), buffer.size(), 0);
        if (bytes_received <= 0) {
            break;
        }
        
        string message = username + ": " + string(buffer.begin(), buffer.begin() + bytes_received);
        broadcast_message(message, client_socket);
    }
    
    {
        lock_guard<mutex> lock(clients_mutex);

        // Remove the client from the clients vector, using the socket as the key 
        clients.erase(remove_if(clients.begin(), clients.end(),
            [client_socket](const Client& c) { return c.socket == client_socket; }),
            clients.end()); 
    }

    // Send updated users list after client disconnection
    send_connected_users_list();
    
    #ifdef _WIN32
    closesocket(client_socket);
    #else
    close(client_socket);
    #endif
}

int main() {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return 1;
    }
    #endif

    int server_fd;  // File descriptor for the server socket
    struct sockaddr_in address; // Server address
    int opt = 1;    // Option value for setsockopt
    int addrlen = sizeof(address);
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Forcefully attaching socket to the port 8080
    #ifdef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt))) {
    #else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    #endif
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;   // IPv4
    address.sin_addr.s_addr = INADDR_ANY;   // Accept connections from any IP address
    address.sin_port = htons(PORT); // Port to listen on
    
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // Add some test user credentials
    user_credentials["Dina Walid"] = "1234";
    user_credentials["Youssef Mohamed"] = "000000";
    user_credentials["Mahmoud Shaban"] = "000000";
    user_credentials["Ali"] = "1234";
    user_credentials["Abdelrahman"] = "1234";
    user_credentials["Omar"] = "1234";

    
    cout << "Server is running and listening on port " << PORT << endl;
    
    while (true) {
        int new_socket;
        // Accept a new connection
        if ((new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrlen))) < 0) {
            perror("accept");
            continue;
        }
        
        // Create a new thread to handle the client simultaneously
        thread client_thread(handle_client, new_socket);
        client_thread.detach();
    }
    
    #ifdef _WIN32
    WSACleanup();
    #endif

    return 0;
}