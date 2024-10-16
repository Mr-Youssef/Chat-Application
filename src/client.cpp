#include <wx/wx.h>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

// Platform-specific macros and types
#ifdef _WIN32
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET(s) closesocket(s)
    typedef int socklen_t;
#else
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET(s) close(s)
#endif

using namespace std;

const wxString SERVER_IP = "127.0.0.1"; // Localhost
const int SERVER_PORT = 8080;        // Port to connect to

// Define colors for dark mode
const wxColour DARK_BG(0, 0, 0);
const wxColour DARK_CTRL_BG(34, 34, 34);
const wxColour DARK_BUTTON_BG(34, 34, 34);

// Text colors
const wxColour TEXT_NORMAL(220, 220, 220);  // Light grey for normal text
const wxColour TEXT_HIGHLIGHT(255, 255, 255);  // White for highlighted text
const wxColour TEXT_ACCENT(100, 200, 255);  // Light blue for accent text
const wxColour TEXT_CHATLOG(62, 214, 200);  // Cyan for chat log text

class ChatClient : public wxFrame {
public:
    ChatClient() : wxFrame(NULL, wxID_ANY, "Chat Client"), socket(INVALID_SOCKET) {
        wxPanel* panel = new wxPanel(this, wxID_ANY);
        panel->SetBackgroundColour(DARK_BG);
        
        // Login controls
        usernameCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(10, 10), wxSize(150, 25));
        passwordCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(10, 40), wxSize(150, 25), wxTE_PASSWORD);
        loginButton = new wxButton(panel, wxID_ANY, "Login", wxPoint(170, 10), wxSize(70, 25));

        // Connected users list
        usersList = new wxListBox(panel, wxID_ANY, wxPoint(280, 70), wxSize(110, 335));
        
        // Chat controls
        chatLog = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(10, 70), wxSize(260, 300), wxTE_MULTILINE | wxTE_READONLY);
        messageCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(10, 380), wxSize(180, 25));
        sendButton = new wxButton(panel, wxID_ANY, "Send", wxPoint(200, 380), wxSize(70, 25));

        // Apply dark mode styles
        ApplyDarkModeStyles();
        
        // Event bindings
        loginButton->Bind(wxEVT_BUTTON, &ChatClient::OnLogin, this);
        sendButton->Bind(wxEVT_BUTTON, &ChatClient::OnSend, this);
        
        SetClientSize(400, 420);    // Set window size

        // Initialize Winsock
        #ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                wxMessageBox("Failed to initialize Winsock", "Error", wxOK | wxICON_ERROR);
            }
        #endif
    }
    
    ~ChatClient() {
        CloseSocket();      // Close the socket before destroying the frame
        #ifdef _WIN32
            WSACleanup();   // Cleanup Winsock
        #endif
    }
    
private:
    wxTextCtrl* usernameCtrl;
    wxTextCtrl* passwordCtrl;
    wxButton* loginButton;
    wxListBox* usersList;
    wxTextCtrl* chatLog;
    wxTextCtrl* messageCtrl;
    wxButton* sendButton;
    SOCKET socket;
    thread receiveThread;
    bool connected = false;

    void ApplyDarkModeStyles() {
        this->SetBackgroundColour(DARK_BG);
        
        usernameCtrl->SetBackgroundColour(DARK_CTRL_BG);
        usernameCtrl->SetForegroundColour(TEXT_NORMAL);
        passwordCtrl->SetBackgroundColour(DARK_CTRL_BG);
        passwordCtrl->SetForegroundColour(TEXT_NORMAL);
        
        loginButton->SetBackgroundColour(DARK_BUTTON_BG);
        loginButton->SetForegroundColour(TEXT_HIGHLIGHT);
        
        usersList->SetBackgroundColour(DARK_CTRL_BG);
        usersList->SetForegroundColour(TEXT_ACCENT);
        
        chatLog->SetBackgroundColour(DARK_CTRL_BG);
        chatLog->SetForegroundColour(TEXT_CHATLOG);
        
        messageCtrl->SetBackgroundColour(DARK_CTRL_BG);
        messageCtrl->SetForegroundColour(TEXT_NORMAL);
        
        sendButton->SetBackgroundColour(DARK_BUTTON_BG);
        sendButton->SetForegroundColour(TEXT_HIGHLIGHT);
    }

    void AppendColoredText(const wxString& text, const wxColour& color) {
        long insertionPoint = chatLog->GetInsertionPoint();
        chatLog->AppendText(text);
        long end = chatLog->GetInsertionPoint();
        chatLog->SetStyle(insertionPoint, end, wxTextAttr(color));
    }
    
    void OnLogin(wxCommandEvent& event) {
        wxString username = usernameCtrl->GetValue();
        wxString password = passwordCtrl->GetValue();
        
        if (username.IsEmpty() || password.IsEmpty()) {
            wxMessageBox("Please enter both username and password", "Error", wxOK | wxICON_ERROR);
            return;
        }
        
        socket = ::socket(AF_INET, SOCK_STREAM, 0); // Create a new socket
        if (socket == INVALID_SOCKET) {
            wxMessageBox("Failed to create socket", "Error", wxOK | wxICON_ERROR);
            return;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;    // IPv4
        serverAddr.sin_port = htons(SERVER_PORT);   // Port to connect to
        inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddr.sin_addr);    // Convert IP address to binary form

        // Connect to the server
        if (connect(socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            wxMessageBox("Failed to connect to server", "Error", wxOK | wxICON_ERROR);
            closesocket(socket);
            return;
        }

        wxString auth_data = username + ":" + password;
        send(socket, auth_data.c_str(), auth_data.Length(), 0);
        
        char buffer[4096] = {0};  // Increased buffer size to accommodate chat history
        int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            wxString response(buffer, wxConvUTF8);
            if (response.StartsWith("Welcome")) {
                connected = true;
                chatLog->AppendText(response + "\n");

                // Receive and display chat history
                bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
                if (bytesReceived > 0) {
                    wxString history(buffer, wxConvUTF8);
                    chatLog->AppendText(history + "\n");
                }

                // Start a new thread to receive messages from the server asynchronously
                receiveThread = thread(&ChatClient::ReceiveMessages, this);
                receiveThread.detach();
            } else {
                wxMessageBox("Authentication failed", "Error", wxOK | wxICON_ERROR);
                closesocket(socket);
            }
        } else {
            wxMessageBox("Failed to receive response from server", "Error", wxOK | wxICON_ERROR);
            closesocket(socket);
        }
    }
    
    void OnSend(wxCommandEvent& event) {
        if (!connected) {
            wxMessageBox("Please login first", "Error", wxOK | wxICON_ERROR);
            return;
        }
        
        wxString message = messageCtrl->GetValue();
        if (!message.IsEmpty()) {
            send(socket, message.c_str(), message.Length(), 0);
            messageCtrl->Clear();
        }
    }
    
    void ReceiveMessages() {
        char buffer[1024];
        while (connected) {
            int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                wxString message(buffer, wxConvUTF8, bytesReceived);
                if (message.StartsWith("Connected:")) { 
                    wxString userListStr = message;
                    wxTheApp->CallAfter([this, userListStr]() {
                        UpdateUserList(userListStr);
                    });
                } else {
                    wxTheApp->CallAfter([this, message]() {
                        AppendColoredText(message + "\n", TEXT_CHATLOG);
                    });
                }
            } else if (bytesReceived == 0) {
                connected = false;
                wxTheApp->CallAfter([this]() {
                    wxMessageBox("Disconnected from server", "Error", wxOK | wxICON_ERROR);
                });
                break;
            } else {
                int error = WSAGetLastError();

                #ifdef _WIN32
                if (error == WSAECONNABORTED) { // If Connection was closed
                #else
                if (error == ECONNABORTED) {
                #endif
                    wxTheApp->CallAfter([this]() {
                        wxMessageBox("Disconnected from server", "Disconnected", wxOK);
                    });
                } else {
                    // Display error message for unexpected errors
                    connected = false;
                    wxTheApp->CallAfter([this]() {
                        wxMessageBox("Error receiving message", "Error", wxOK | wxICON_ERROR);
                    });
                    break;
                }

            }
            #ifdef _WIN32
                Sleep(100); // Sleep for 100ms, to avoid busy-waiting
            #else
                usleep(100000);
            #endif
        }
    }

    void UpdateUserList(const wxString& userListStr) {
        // Extract the first part of the message till the first newline character
        // this will be "Connected: " followed by the number of connected users
        wxString message = userListStr.BeforeFirst('\n');
        usersList->Clear();
        usersList->Append(message);

        message = userListStr.AfterFirst('\n'); 
        // Extract usernames from the message
        wxArrayString users = wxSplit(message, ',');
        for (const auto& user : users) {
            usersList->Append(user);
        }
    }

    void CloseSocket() {
        if (socket != INVALID_SOCKET) {
            closesocket(socket);
            socket = INVALID_SOCKET;
        }
        connected = false;
    }

};

class ChatApp : public wxApp {
public:

    // Overriding the OnInit method to create and show the ChatClient window
    virtual bool OnInit() {
        ChatClient* frame = new ChatClient();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(ChatApp);   // This macro implements the necessary entry point for a wxWidgets application
