#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <poll.h>
#include <errno.h>
#include <map>
#include <fstream>
#include <algorithm>


using namespace std;


#define BACKLOG  5                      // Allowed length of queue of waiting connections

const string SERVER_ID = "G_uinness";
const vector<Server> CONNECTED_SERVERS;
const string TSAM_SERVER_IP = "130.208.246.98";
const int TSAM_SERVER_PORT = 5001;
class Client
{
    public:
    int sock;
    string name;
    unsigned char recvbuffer[5000];
    unsigned char sendbuffer[5000];
    int recvd_bytes;

    Client(int socket) : sock(socket){};
    ~Client(){};
};


struct friend_server
{
    string g_id;
    string ip;
    int port;
};


class Server
{
    public:
    int sock;
    string ip;
    int port;
    string g_id;
    vector<Server*> connected_servers;
    Server(int sock_fd, string s_name, string ip_addr, int port) : sock(sock_fd), g_id(s_name), ip(ip_addr), port(port){};
    ~Server(){};
};


string getTimestamp() {
    time_t now = time(nullptr);
    tm* localTime = localtime(&now);

    stringstream ss;
    ss << put_time(localTime, "%d/%m/%Y %H:%M:%S");

    return ss.str();
}


map<int, Client*> clients; // Lookup table for per Client information


// list of poll file descriptors.
// Index 0 is always the listening socket, all others are connected clients.
vector<struct pollfd> fds;


struct command_input{
    string cmd;
    string from;
    string to;
    string msg;
};


struct unread_message{
    string message;
    string from;
};


map<string,vector<unread_message> > UNREAD_MESSAGES;


int nfds = 1;
int open_socket(int port){
    int sock;
    int set = 1;
    sockaddr_in sk_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("failed to open Socket");
        return (-1);
    }
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
        close(sock);
    }

    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family       = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons(port);
    if(::bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0){
        perror("error binding socket");
        return (-1);
    }

    return sock;
}


void add_clients(int listening_sock, sockaddr_in client, size_t client_len, vector<struct pollfd>& fds){ //map<int, Client*>* clients
    printf("Client connected!\n");

              // While there is some client waiting to connect, accept that connection
    int client_sock = accept(listening_sock, (sockaddr*) &client,(socklen_t*) &client_len);
    //TODO: if no username, fuck you
    fds.push_back({client_sock, POLLIN, 0});
    Client *new_client = new Client(client_sock);
    new_client->recvd_bytes = 0;
    memset(new_client->recvbuffer, 0, sizeof(new_client->recvbuffer));
    memset(new_client->sendbuffer, 0, sizeof(new_client->sendbuffer));
    
    clients[client_sock] = new_client;
    nfds++;
}


void remove_clients(Client* client){
    int client_socket = client->sock;
    close(client_socket);

    auto fdIt = find_if(fds.begin(), fds.end(),
                [client_socket](const struct pollfd &pfd)
                {
                    return pfd.fd == client_socket;
                });

    if(fdIt != fds.end())
    {
        fds.erase(fdIt);
    }
}


command_input extract_command(char* message){
    stringstream ss(message);
    command_input command{};

    if (!getline(ss, command.cmd, ',')) return {};
    if (command.cmd == "SENDMSG"){
        if (!getline(ss, command.from, ',')) return {};
        if (!getline(ss, command.to, ',')) return {};
        if (!getline(ss, command.msg)) return {};
    } else if (command.cmd == "GETMSG"){
        if (!getline(ss, command.from, ',')) return {};
    } else if (command.cmd == "STATUSREQ") return command;
    else if (command.cmd == "USERNAME"){
        if (!getline(ss, command.from)) return {};
    } else return {};
    return command;
}


int sendMessage(command_input &command){ // store a message on the server
    unread_message new_message;
    new_message.message = command.msg;
    new_message.from = command.from;
    if (UNREAD_MESSAGES.count(command.to) > 0){
        UNREAD_MESSAGES[command.to].push_back(new_message);
    } else {
        vector<unread_message> entry = {new_message};
        UNREAD_MESSAGES[command.to] = entry;
    } printf("MESSAGE SENT\n");
    return 0;
}


string getMessage(string client, string from){
    /* Returns the most recent unread messages from a specified client */
    if (UNREAD_MESSAGES.count(client) == 0) return "EMPTY";
    string client_message;
    vector<unread_message>& client_message_vector = UNREAD_MESSAGES[client];
    for (int i = 0; i < client_message_vector.size(); i++){
        if (client_message_vector[i].from == from){
            client_message = client_message_vector[i].message;
            client_message_vector.erase(client_message_vector.begin() + i);
        }
    }
    return client_message;
}


string statusRequest(){
    printf("STATUSREQ requested\n");
    string message_statuses;
    int i = 1;
    for (const auto& [name, messages] : UNREAD_MESSAGES){
        int message_len = messages.size();
        if (message_len > 0){
            message_statuses += (name + ":" + to_string(message_len));
            i++;
            if (i != 1) message_statuses += ",";
        }
    }
    return message_statuses;
}


string createFrame(string message){

    //Create the frame that we edit and then return
    string frame = "";

    uint16_t length = message.size() + 5; // add 5 because of SOH,length(2bytes),STX and ETX

    frame += (char)0x01; //SOH

    frame += (char)(length >> 8); //Get the first byte
    frame += (char)(length & 0xFF); //Get the second byte

    frame += (char)0x02; //STX

    frame += message; //Add the message

    frame += (char)0x03; //ETX

    return frame;
}


bool sendToClient(Client* client, string message){
    string frame = createFrame(message);
    int sent = send(client->sock, frame.data(), frame.size(), 0);
    if (sent < 0) return false;
    return true;
}


// bool sendToServer(Server* server, string message){

// }


void writeLog(ofstream& logFile, string message){
    logFile << "| " << getTimestamp() << " | " << message << endl;
}




void connecetToServer(string ip, int port){


    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0){
        perror("Error creating socket");
        return -1;
    }

    //Set server IP and port
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);



    
    

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1){
        cerr << "Invalid server IP address to connect to" << endl;
        close(sock);
    }

    
    
    //Connect to the chosen server
    if(connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Error conectiong to server");
        close(sock)
    }
    writeLog(logFile, "Connected to server");
    



    //send HELO
    helo();
    



    //Receive HELO
    char buffer[1024];

    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received < 0) {
        perror("Error receiving HELO");
    }


    //If helo is received
    else {
        buffer[bytes_received] = '\0';

        string group_id = buffer;
        


        server = Server(sock, group_id, ip, port) //Create the server
        CONNECTED_SERVERS.push_back(server) //Add the server to our list of connected servers
    
       servers() //Send back SERVERS
    }

}

    
    
    
    void acceptServer(){

}

void helo(){
    string our_group_id = uinness;


    send()


    
}

void servers(){
   
    
    extract_command()
}










int main(int argc, const char* argv[]){
    if (argc != 2){
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }
    ofstream logFile("server.log", ios::app);
    writeLog(logFile,  "Server started");
    int port = stoi(argv[1]);
    struct sockaddr_in client_sock_addr;
    socklen_t client_len;
    int client_sock;                        // Socket of connecting client
    
    writeLog(logFile, "Setting up listening socket...");
    
    
    int listening_sock = open_socket(port);
    if (listening_sock < 0){
        perror("Error creating listening socket");
        exit(1);
    }
    
    if (listen(listening_sock, BACKLOG) < 0){
        printf("Listen failed at %d\n", port);
        close(listening_sock);
        exit(0);
    } 
    else 
    // Add listen socket to the list of fds being polled.
{
    fds.push_back({listening_sock, POLLIN});
    writeLog(logFile, "Server listening on port " + port);
}

//bool finished = false;
while (true){
         
        // Wait for something to read
        writeLog(logFile, "Server waiting for data");
        int n = poll(fds.data(), fds.size(), -1);

        if (n < 0){
            perror("Polling failed - Closing down\n");
            close(listening_sock);
            exit(1);
        }

        vector<struct pollfd> readyFds = fds;

        // Check if the listening socket has new readable data
        if (fds[0].revents & POLLIN){
            client_len = sizeof(client_sock_addr);
            add_clients(listening_sock, client_sock_addr, client_len, fds);
            writeLog(logFile, "Client connected");
        }
        for(size_t i = 1; i < readyFds.size(); i++){
            if(readyFds[i].revents & POLLIN){
                int client_sock = readyFds[i].fd;
                Client *client = clients[client_sock];
                unsigned char *buffer = client->recvbuffer;
            // Keep recieving until there is nothing more to recieve
            int recvd;
                recvd = recv(client_sock, buffer + client->recvd_bytes, sizeof(client->recvbuffer) - client->recvd_bytes, MSG_DONTWAIT);                        
                // if it's a client disconnectiong, log it and remove from vector

                // in this case we don't have any command to deal with, so we move on to the next fd
                if (recvd == 0) {
                    remove_clients(client);
                    writeLog(logFile, "Client disconnected");
                    continue;
                }
                client->recvd_bytes += recvd;
                
                // if we don't have at least 4 bytes, we can't know what the length of the frame should be so we move on
                if (client->recvd_bytes < 4) continue;

                while (client->recvd_bytes >= 4) {
                    // Extract frame length
                    uint16_t length;
                    memcpy(&length, buffer + 1, sizeof(length));

                    size_t frame_len = ntohs(length);

                    // Validate frame length
                    if (frame_len < 5 || frame_len > sizeof(client->recvbuffer)) {
                        // Handle invalid frame, e.g. disconnect client
                        break;
                    }

                    // Not enough bytes for the complete frame yet
                    if (client->recvd_bytes < frame_len) {
                        break;
                    }

                    // Extract command
                    size_t command_byte_len = frame_len - 5;

                    vector<unsigned char> message(command_byte_len + 1);

                    memcpy(message.data(), buffer + 4, command_byte_len);
                    message[command_byte_len] = '\0';

                    command_input command = extract_command((char*)message.data());
                
                    
                if (command.cmd == "SENDMSG"){
                    if (command.from == "" || command.to == "" || command.msg == ""){
                        printf("Incorrectly formatted command. Usage: SENDMSG,<from>,<to>,<msg>\n");
                        continue;
                    }
                    if (sendMessage(command) < 0){
                        cout << ("Failed to send message to " + (string) command.to);
                        continue;
                    }
                    writeLog(logFile, "Command: SENDMSG. " + client->name + " has sent a message to " + command.to);
                    sendToClient(client, "OK");
                    writeLog(logFile, "Response sent: 'OK'");

                } else if (command.cmd == "GETMSG"){
                    if (command.from == ""){
                        printf("Incorrectly formatted command. Usage: GETMSG,<from>\n");
                        continue;
                    }
                    string gotten_message = getMessage(client->name, command.from);
                    sendToClient(client, gotten_message);
                    writeLog(logFile, "Command: GETMSG. " + client->name + " requested messages from " + command.from);

                } else if (command.cmd == "STATUSREQ"){
                    string status = statusRequest();
                    if (status == "") status = "EMPTY";
                    sendToClient(client, status);
                    writeLog(logFile, "Command: STATUSREQ. Status requested");
                } else if (command.cmd == "USERNAME"){
                    client->name = command.from;
                    sendToClient(client, "Welcome");
                    writeLog(logFile, "Username Recieved: " + client->name);
                    writeLog(logFile, client->name + " has logged in!"); 
                } else {
                    sendToClient(client, "INVALID COMMAND");
                    writeLog(logFile, "User entered invalid command");
                }

                    // Remove the processed frame from the buffer
                size_t remaining = client->recvd_bytes - frame_len;

                memmove(buffer, buffer + frame_len, remaining);

                client->recvd_bytes = remaining;
            }
        }
    }
}
}

