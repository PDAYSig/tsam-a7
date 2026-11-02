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
#include <ctime>
#include <iomanip>


using namespace std;


#define BACKLOG  5                      // Allowed length of queue of waiting connections

const string SERVER_ID = "G_uinness";
const string TSAM_SERVER_IP = "130.208.246.98";
const int TSAM_SERVER_PORT = 5001;
const string SERVER_IP = "130.208.246.98";
struct command_input{
    string cmd;
    string from;
    string to;
    string timestamp;
    string msg;
};
struct unread_message{
    string message;
    string from;
    string to;
    string timestamp;
};
map<string,vector<unread_message> > UNREAD_MESSAGES;
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
    unsigned char recvbuffer[5000];
    unsigned char sendbuffer[5000];
    vector<string> connected_servers;

    Server(int sock_fd, string s_name, string ip_addr, int port) : sock(sock_fd), g_id(s_name), ip(ip_addr), port(port){};
    ~Server(){};
};

vector<Server> CONNECTED_SERVERS;
vector<friend_server> FRIEND_SERVERS;


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


void add_connections(int listening_sock, sockaddr_in client, size_t client_len, vector<struct pollfd>& fds){     
    int client_sock = accept(listening_sock, (sockaddr*) &client,(socklen_t*) &client_len);
    printf("New connection\n");
    fds.push_back({client_sock, POLLIN, 0});
   
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


void remove_server(Server* server) {
    int server_socket = server->sock;
    close(server_socket);

    auto fdit = find_if(fds.begin(), fds.end(),
        [server_socket](const struct pollfd &pfd) {
            return pfd.fd == server_socket;
        }
    );

    if (fdit != fds.end()) {
        fds.erase(fdit);
    }

    auto it = find_if(CONNECTED_SERVERS.begin(), CONNECTED_SERVERS.end(),
        [server](Server &s) {
            return &s == server;
        }
    );

    if (it != CONNECTED_SERVERS.end()) {
        CONNECTED_SERVERS.erase(it);
    }
}


command_input extract_command(char* message, bool from_server = false){
    stringstream ss(message);
    command_input command{};
    
    if (!getline(ss, command.cmd, ',')) return {};
    if (command.cmd == "SENDMSG"){
        if (!getline(ss, command.from, ',')) return {};
        if (!getline(ss, command.to, ',')) return {};
        
        if (from_server){
            //Message is from another server and has a timestamp
            if (!getline(ss, command.timestamp, ',')) return {};
        }
        else{
            //Create a new timestamp for client message
            command.timestamp = to_string(time(nullptr));
        }
        if (!getline(ss, command.msg)) return {};
    } else if (command.cmd == "GETMSG"){
        if (!getline(ss, command.from, ',')) return {};
    } else if (command.cmd == "STATUSREQ") return command;
    else if (command.cmd == "USERNAME"){
        if (!getline(ss, command.from)) return {};
    } else return {};
    return command;
}







string getMessage(string from){

    if (UNREAD_MESSAGES.count(SERVER_ID) == 0)
        return "EMPTY";

    vector<unread_message>& messages = UNREAD_MESSAGES[SERVER_ID];

    for (int i = 0; i < messages.size(); i++){

        if (messages[i].from == from){

            string result = messages[i].message;

            messages.erase(messages.begin() + i);

            return result;
        }
    }

    return "EMPTY";
}


string statusRequest(){
    printf("STATUSREQ requested\n");

    string message_statuses;
    map<string, int> counts;


    // Only check messages addressed to our server
    for (const unread_message& msg : UNREAD_MESSAGES[SERVER_ID]){
        counts[msg.from]++;
    }


    for (const auto& [name, count] : counts){
        if (!message_statuses.empty()){
            message_statuses += ",";
        }

        message_statuses += (name + ":" + to_string(count));        
    }
    if (message_statuses.empty()){
        return "EMPTY";
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


bool sendToServer(Server* server, string message){
    string frame = createFrame(message);
    int sent = send(server->sock, frame.data(), frame.size(), 0);
    if (sent < 0) return false;
    return true;
}


// Returns the oldest message a group has on this server
unread_message getmsgs(string g_id){
    if (UNREAD_MESSAGES.count(g_id) > 0){
        vector<unread_message> messages = UNREAD_MESSAGES[g_id];
        
        unread_message oldest = messages.front();
        messages.erase(messages.begin());
        return oldest;
    }
    unread_message empty_message = {};
    return empty_message;
}

void writeLog(ofstream& logFile, string message){
    logFile << "| " << getTimestamp() << " | " << message << endl;
}    


// Send a message to a server
void serverSendmsg(Server* server, unread_message msg){
    string message = "SENDMSG,";
    message += msg.from + ',';
    message += msg.to + ',';
    message += msg.timestamp + ',';
    message += msg.message;

    sendToServer(server, message);
}


//Send our group id to another server after connecting
void helo(int sock){
    string message = "HELO," + SERVER_ID;

    string frame = createFrame(message);
    
    if (send(sock, frame.data(), frame.size(), 0) < 0){
        perror("Error sending HELO");
    }    
}


//Tell another server all of our connected servers
string servers(int sock, int our_port){

    string all_servers = "SERVERS," + SERVER_ID + "," + SERVER_IP + "," + to_string(our_port) + ";"; //Also add our server

    //Build the list by adding to the string valid group id's and known addresses
    for (const Server& server : CONNECTED_SERVERS) {

        if (server.g_id.empty()){
            continue;
        }
        
        if (server.ip.empty() || server.port == 0){
            continue;
        }
        all_servers += server.g_id + "," + server.ip + "," + to_string(server.port) + ";";
    }


    //remove the final semicomma
    if (all_servers.back() == ';') {
        all_servers.pop_back();
    }

    string frame = createFrame(all_servers);

    if (send(sock, frame.data(), frame.size(), 0) < 0){
        perror("Error sending SERVERS");
        return "";
    }
    return all_servers;


}


int connectToServer(string ip, int port){

    //Create a IPv4 socket for the connection
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


    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1){
        cerr << "Invalid server IP address to connect to" << endl;
        close(sock);
        return -1;
    }    

    
    
    //Connect to the chosen server
    if(connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Error conectiong to server");
        close(sock);
        return -1;
    }    
    

    //Save the server
    Server server(sock, "", ip, port);
    CONNECTED_SERVERS.push_back(server);
    

    //Let poll() notify main() when this server sends data
    fds.push_back({sock, POLLIN, 0});


    //send HELO
    helo(sock);

    return sock;
}    




void receiveHelo(int sock, string message) {

    //Get the servers group_id
    string group_id = message.substr(5);


    //The serer might be saved but not with the id yet
    for (Server& server : CONNECTED_SERVERS) {

        if (server.sock == sock) {
            server.g_id = group_id;

            return;
        }    
    }    

    //Server does not exist, save it
    Server server(sock, group_id, "", 0);
    CONNECTED_SERVERS.push_back(server);
}    



void receiveServers(int sock, string message) {

    //Find the server that sent the SERVERS, to save who are connected to him
    Server* sender = nullptr;
    for (Server& server : CONNECTED_SERVERS) {
        if (server.sock == sock) {
            sender = &server;
            break;
        }
    }
    if (sender == nullptr) {
        return;
    }





    //Remove the SERVERS, (only get the list of servers)
    string server_list = message.substr(8);

    stringstream ss(server_list);
    string entry;


    //Each server is seperated by a semicomma, get the group_id, ip and port 
    while (getline(ss, entry, ';')) {

        stringstream entry_stream(entry);

        string group_id;
        string ip;
        string port_string;

        if (!getline(entry_stream, group_id, ',')) continue;
        if (!getline(entry_stream, ip, ',')) continue;
        if (!getline(entry_stream, port_string, ',')) continue;

        int port;

        //Skip servers that dont have a valid port 
        try {
            port = stoi(port_string);
        }    
        catch (...) {
            continue;
        }    

        // Check if we already know this server
        bool already_exists = false;

        // Check connected servers
        for (Server& server : CONNECTED_SERVERS) {
            if (server.g_id == group_id) {
                already_exists = true;

                //if we don't know the ip or the port of one of our servers, add them
                if (server.ip == ""){
                    server.ip = ip;
                }
                if (server.port == 0){
                    server.port = port;
                }
                break;
            }    
        }    

        // Check discovered servers
        for (const friend_server& server : FRIEND_SERVERS) {
            if (server.g_id == group_id) {
                already_exists = true;
                break;
            }    
        }    

        // Don't add ourselves or duplicates
        // G_nammi and G_mancheetah were messing up our logs so we blocked them
        if (group_id == SERVER_ID || already_exists || group_id == "G_nammi" || group_id == "G_mancheetah") {
            continue;
        }    

        //Keep track of wich servers are connected to him
        sender->connected_servers.push_back(group_id);
        
        
        // Add newly discovered server
        friend_server new_server;
        new_server.g_id = group_id;
        new_server.ip = ip;
        new_server.port = port;
        
        FRIEND_SERVERS.push_back(new_server);
        
    }    
}    

void keepAlive(int sock, string group_id){
    int count = UNREAD_MESSAGES[group_id].size();

    string message = "KEEPALIVE,0" + to_string(count);
    
    string frame = createFrame(message);
    send(sock, frame.data(), frame.size(), 0);
}




void storeMessage(string from, string to, string timestamp, string message) {

    unread_message new_message = {message, from, to, timestamp};

    UNREAD_MESSAGES[to].push_back(new_message);
}










string statusResp(){
    string message = "STATUSRESP,";
    for (const auto& server : UNREAD_MESSAGES){
        if (!server.second.empty()){
            message += server.first + ":" + to_string(server.second.size()) + ",";
        }
    }
    
    if (message == "STATUSRESP,"){
        return "STATUSRESP,EMPTY";
    }

    message.pop_back(); //Delete the final comma
    return message;
}



void receiveStatusresp(int sock, string message){

    //Remove the "STATUSRESP,"
    string server_list = message.substr(11);

    if (server_list == "EMPTY"){
        return;
    }

    //Get the server that sent statusresp
    string sender_id = "";

    for (const Server& server : CONNECTED_SERVERS){
        if (server.sock == sock){
            sender_id = server.g_id;
            break;
        }
    }





    stringstream ss(server_list);
    string entry;


    while (getline(ss, entry, ',')) {

        stringstream entry_stream(entry);

        string group_id;
        string count_str;
        

        if (!getline(entry_stream, group_id, ':')) continue;
        if (!getline(entry_stream, count_str, ',')) continue;

        int count;
        try {
            count = stoi(count_str); 
        }
        catch(...){
            continue;
        }

        //Check if the server is us
        bool known_server = (group_id == SERVER_ID);

        //Cechk if the server is connected to us
        for (Server& server : CONNECTED_SERVERS){
            if (server.g_id == group_id){
                known_server = true;
                break;
            }
        }

        //Check if our friends know this server
        for (friend_server& server : FRIEND_SERVERS){
            if (server.g_id == group_id){
                known_server = true;
                break;
            }
        }



        //Ask for the messeges that are related to us
        //Send getMSG as many times as messeges we need
        if (known_server && count > 0 && group_id.substr(0,2) != "I_" && group_id != sender_id){
            string command = "GETMSGS," + group_id;
        
            string frame = createFrame(command);
            send(sock, frame.c_str(), frame.size(), 0);
            cout << "sent GETMSGS to " + group_id + '\n';
            }
        }   
    }





map<int, string> receive_buffer;

//Start the server, connect to bootstrap, process incoming commands
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
        
        writeLog(logFile, "Server listening on port " + to_string(port));
    }





    // Connect to TSAM server
    int server_sock = connectToServer(TSAM_SERVER_IP, TSAM_SERVER_PORT);
    cout << "Bootstrap socket: " << server_sock << endl;

    
    if (server_sock < 0){
        cout << "Could not connect to bootstrap server" << endl;
        writeLog(logFile, "Failed to connect to bootstrap server");
    }
    else {
        cout << "Connected to bootstrap server!" << endl;
        writeLog(logFile, "Connected to bootstrap server");
    }
    

    // while (true){

        // Begin by checking if we have the minimum amound of required servers
        
        // //Connect GOATS
        // int friend_sock = connectToServer("ip", 4022);
        // cout << "Goats socket: " << friend_sock << endl;
        
        
        // if (friend_sock < 0) {
            //     cout << "Could not connect to friend's server" << endl;
            //     }
            // else {
                //     cout << "Connected to friend's server!" << endl;
                //     writeLog(logFile, "Connected to friend's server");
                // }
                
    time_t last_keepAlive = time(nullptr);
                
    while (true){
        
        if ((CONNECTED_SERVERS.size() < 8) && (FRIEND_SERVERS.size() > 0)){
            friend_server server_to_connect = FRIEND_SERVERS[0];
            writeLog(logFile, "Attempting to connect to " + server_to_connect.g_id);
            FRIEND_SERVERS.erase(FRIEND_SERVERS.begin());

            if (connectToServer(server_to_connect.ip, server_to_connect.port) < 0){
                writeLog(logFile, "Failed to connect to " + server_to_connect.g_id);
            } else writeLog(logFile, "Connected to " + server_to_connect.g_id);
        }
        
        // Wait for something to read
        int n = poll(fds.data(), fds.size(), 1000); //waits at most 1 sec

        //send keep alive if 1 minute has passed
        if (time(nullptr) - last_keepAlive >= 60){

            for (const Server& server : CONNECTED_SERVERS){
                if (server.sock >= 0){
                    keepAlive(server.sock, server.g_id);
                    writeLog(logFile, "Sent KEEPALIVE to " + server.g_id + " on socket " + to_string(server.sock));



                    //Also send STATUSREQ at the same time as KEEPALIVE
                    string statusreq_message = "STATUSREQ";
                    string statusreq_frame = createFrame(statusreq_message);

                    if (send(server.sock, statusreq_frame.data(), statusreq_frame.size(), 0) < 0){
                        perror("Error sending statusreq");
                    }


                }
            }
            last_keepAlive = time(nullptr);
        }
        

        if (n < 0){
            perror("Polling failed - Closing down\n");
            close(listening_sock);
            exit(1);
        }

        vector<struct pollfd> readyFds = fds;

        // Check if the listening socket has new readable data
        if (fds[0].revents & POLLIN){
            client_len = sizeof(client_sock_addr);
            add_connections(listening_sock, client_sock_addr, client_len, fds); //adds both lients and servers
            writeLog(logFile, "New connection accepted");
        }
        for(size_t i = 1; i < readyFds.size(); i++){
            if(readyFds[i].revents & POLLIN){
                
                int sock = readyFds[i].fd;
                
                char buffer[5000];
                

                //Receive the message
                int received = recv(sock, buffer, sizeof(buffer), 0);

                if (received <= 0) {

                    if (received < 0) {
                        cerr << "Error on socket " << sock << ": ";
                        perror("recv");
                    }
                    else {
                        writeLog(logFile, "Socket disconnected: " + to_string(sock));
                    }

                    // Close the disconnected socket
                    close(sock);

                    // Stop poll() from checking this socket
                    fds.erase(remove_if(fds.begin(), fds.end(),
                        [sock](const pollfd& p) {
                            return p.fd == sock;
                        }), fds.end());

                    // Remove the socket from connected servers
                    CONNECTED_SERVERS.erase(
                        remove_if(CONNECTED_SERVERS.begin(), CONNECTED_SERVERS.end(),
                            [sock](const Server& server) {
                                return server.sock == sock;
                            }), CONNECTED_SERVERS.end());

                    // Remove the socket's saved incoming data
                    receive_buffer.erase(sock);

                    // Remove client information if it exists
                    if (clients.count(sock)) {
                        delete clients[sock];
                        clients.erase(sock);
                    }

                    continue;
                }


                //TCP might not send all of the message at once
                receive_buffer[sock].append(buffer, received);
                string& data = receive_buffer[sock];
                while(!data.empty()){
                    string message;

                    // A valid frame starts with SOH (0x01); discard stray bytes.
                    if ((unsigned char)data[0] != 0x01) {
                        data.erase(0, 1);
                        continue;
                    }

                    // Wait until the SOH and both length bytes have arrived.
                    if (data.size() < 3) break;

                    // Read the two-byte big-endian total frame length
                    size_t length =
                        ((unsigned char)data[1] << 8) |
                        (unsigned char)data[2];

                    if (length < 5 || length > 5000) {
                        data.erase(0, 1);
                        continue;
                    }

                    // Keep waiting if we dont have the full message
                    if (data.size() < length) break;


                    // Check the STX and ETX markers before accepting the frame.
                    if ((unsigned char)data[3] != 0x02 ||
                        (unsigned char)data[length - 1] != 0x03) {
                        data.erase(0, 1);
                        continue;
                    }


                    /* ------- No incomplete commands get past here ------- */
                    // Extract the command and remove the frame from the buffer.
                    message = data.substr(4, length - 5);
                    data.erase(0, length);



                    //After this is checking wich command was used

                    if (message.substr(0, 5) == "HELO,") {
                        writeLog(logFile, "Received HELO: " + message);

                        cout << "Received HELO from socket " << sock
                            << ": " << message << endl;

                        bool already_connected = any_of(
                            CONNECTED_SERVERS.begin(),
                            CONNECTED_SERVERS.end(),
                            [sock](const Server& server) {
                                return server.sock == sock;
                            }
                        );

                        if (!already_connected && CONNECTED_SERVERS.size() >= 8) {
                            auto it = find_if(
                                CONNECTED_SERVERS.begin(),
                                CONNECTED_SERVERS.end(),
                                [sock](const Server& server) {
                                    return server.sock != sock &&
                                        server.g_id.substr(0, 2) == "I_";
                                }
                            );

                            if (it == CONNECTED_SERVERS.end()) {
                                it = find_if(
                                    CONNECTED_SERVERS.begin(),
                                    CONNECTED_SERVERS.end(),
                                    [sock](const Server& server) {
                                        return server.sock != sock;
                                    }
                                );
                            }

                            if (it != CONNECTED_SERVERS.end()) {
                                string removed_id = it->g_id;
                                remove_server(&*it);
                                writeLog(logFile, "Removed server: " + removed_id);
                            }
                        }

                        receiveHelo(sock, message);

                        writeLog(logFile, "Processed HELO for peer on socket " +
                                        to_string(sock) + ": " + message.substr(5));

                        if (!already_connected) {
                            helo(sock);
                        }

                        string response = servers(sock, port);
                        writeLog(logFile, "We sent servers: " + response);
                    }

                    else if (message.substr(0, 8) == "SERVERS,"){
                        writeLog(logFile, "Received SERVERS: " + message);
                        receiveServers(sock, message);
                    }

                    else if (message.substr(0, 10) == "KEEPALIVE,"){
                        writeLog(logFile, "Received KEEPALIVE from socket " + to_string(sock) + ": " + message);                        

                        int count = stoi(message.substr(10));

                        if (count > 0){
                            string command = "GETMSGS," + SERVER_ID;
                            string frame = createFrame(command);

                            send(sock, frame.c_str(), frame.size(), 0);

                            writeLog(logFile, "Sent GETMSGS to socket " + to_string(sock));
                        }
                        writeLog(logFile, "Received KEEPALIVE: " + message);
                    }

                    else if (message.substr(0, 8) == "GETMSGS,"){
                        writeLog(logFile, "Received GETMSGS: " + message);
                        string to = message.substr(8, length);
                        if (to == SERVER_ID){
                            writeLog(logFile, "A server is trying to steal our message!");
                            // Block server
                        }
                        unread_message msg_to_get = getmsgs(to);
                        for (Server& server : CONNECTED_SERVERS) {
                            if (server.sock == sock){
                                if (msg_to_get.message.empty()) sendToServer(&server, "EMPTY");
                                else {
                                serverSendmsg(&server, msg_to_get);
                                writeLog(logFile, "Message delivered");
                                }
                            }
                        }
                    }
                    


                    //Client stuff
                    else if (message.substr(0, 9) == "USERNAME,"){                       

                        Client *client = new Client(sock);

                        client->recvd_bytes = 0;
                        memset(client->recvbuffer, 0, sizeof(client->recvbuffer));
                        memset(client->sendbuffer, 0, sizeof(client->sendbuffer));
                        
                        clients[sock] = client;


                        client->name = message.substr(9);

                        sendToClient(client, "Welcome");

                        writeLog(logFile, "Username Recieved: " + client->name);
                        writeLog(logFile, client->name + " has logged in!"); 
                    }

                    else if (message.substr(0, 8) == "SENDMSG,"){
                        command_input command = extract_command((char*)message.c_str(), !clients.count(sock));

                        if (command.from.empty() || command.to.empty() || command.msg.empty() || command.timestamp.empty()){
                            cout << "Incorrectly SENDMSG" << endl;
                            writeLog(logFile, "Invalid SENDMSG received on socket " + to_string(sock));

                        }
                        else {

                            // Create the correct server-to-server message with timestamp
                            string server_message = "SENDMSG," + command.from + "," +
                                command.to + "," +
                                command.timestamp + "," +
                                command.msg;



                            if (clients.count(sock)){ //message from a client
                                Client *client = clients[sock];
                                sendToClient(client, "OK");

                                if (command.to == SERVER_ID){
                                storeMessage(command.from, command.to, command.timestamp, command.msg);
                                continue;
                                }
                                bool known = false;
                                for (Server& server: CONNECTED_SERVERS){
                                    if (server.g_id == command.to) {
                                        known = true;
                                        sendToServer(&server, server_message);
                                        writeLog(logFile, "Routed message for " + command.to + "to port "+ to_string(sock));
                                        break;

                                    } else {
                                        for (string f_server : server.connected_servers){
                                            if (command.to == f_server) {
                                                known = true;
                                                sendToServer(&server, server_message);
                                                writeLog(logFile, "Routed message for " + command.to + "to port "+ to_string(sock));
                                                break;
                                            }
                                        }
                                        if (knwon) break;
                                    }
                                }
                                if (!known){
                                    storeMessage(command.from, command.to, command.timestamp), command.msg);
                                    writeLog(logFile, "Received SENDMSG from client on socket " + to_string(sock) + " for " + command.to);
                                }
                                writeLog(logFile, "Client " + client->name + " sent a message to " + command.to);
                            }

                            else{
                                //Message from a server
                                writeLog(logFile, "Received SENDMSG from server on socket " + to_string(sock) + " for " + command.to);

                                if (command.to == SERVER_ID){
                                    // if it's for us, we save it
                                    storeMessage(command.from, command.to, command.timestamp, command.msg);
                                    continue;
                                }
                                // Check if we know where the message is going
                                bool known = false;
                                for (Server& server: CONNECTED_SERVERS){
                                    if (server.sock == sock) continue; //So we dont send it right back
                                    if (server.g_id == command.to) {
                                        known = true;
                                        sendToServer(&server, server_message);
                                        writeLog(logFile, "Routed message for " + command.to + "to port "+ to_string(sock));
                                        break;

                                    } else {
                                        for (string f_server : server.connected_servers){
                                            if (command.to == f_server) {
                                                known = true;
                                                sendToServer(&server, server_message);
                                                writeLog(logFile, "Routed message for " + command.to + "to port "+ to_string(sock));
                                                break;
                                            }
                                        }
                                        if (known) break;
                                    }
                                }
                                if (!known){
                                    storeMessage(command.from, command.to, command.timestamp, command.msg);
                                    writeLog(logFile, "Received SENDMSG from server on socket " + to_string(sock) + " for " + command.to);
                                }
                            }
                        }
                    }

                    else if (message.substr(0, 7) == "GETMSG,"){


                        Client *client = clients[sock];
                        command_input command = extract_command((char*)message.c_str());

                        if (command.from.empty()){
                            cout << "Incorrectly formatted GETMSG" << endl;
                            writeLog(logFile, "Invalid GETMSG received on socket " + to_string(sock));


                        }
                        else {
                            string gotten_message = getMessage(command.from);

                            sendToClient(client, gotten_message);

                            writeLog(logFile, "Command: GETMSG. " + client->name +
                                            " requested messages from " + command.from);
                        }
                    }   

                    else if (message == "STATUSREQ") {
                        writeLog(logFile, "Received STATUSREQ on socket " + to_string(sock));


                       if (clients.count(sock)) { //Client asks for statusreq
                            
                            Client* client = clients[sock];
                            
                            string response = statusRequest();
                            sendToClient(client, response);
                            
                        }
                        else { //Server asked for statusreq
                            cout << "STATUSREQ received from another server" << endl;
                            string message = statusResp();

                            string frame = createFrame(message);
                            if (send(sock, frame.data(), frame.size(), 0 ) < 0){
                                perror("Error sending STATUSRESP");
                            }
                            writeLog(logFile, "Sent STATUSRESP: " + message);
                        }
                    }
                    else if (message.substr(0, 11) == "STATUSRESP,"){
                        writeLog(logFile, "Received STATUSRESP on socket " + to_string(sock) + ": " + message);
                        receiveStatusresp(sock, message);
                    }
                    else {
                        if (clients.count(sock)){
                            sendToClient(clients[sock], "INVALID COMMAND");
                        }
                        writeLog(logFile, "Invalid command received: " + message);
                    }
                }
            }
        }
    }
}


