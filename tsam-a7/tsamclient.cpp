#include <string>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

using namespace std;


int connectToServer(string ip, int port){
    // Create a socket
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
        cerr << "Invalid IP address" << endl;
        close(sock);
        return -1;
    }

    //Connect to the server

    if(connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Error connecting to server");
        close(sock);
        return -1;
    }

    cout << "Connected to Server" << endl;

    return sock;
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



bool sendMessage(int sock, string message){
    
    if (message.size() > 4995){
        cerr << "Message is too long" << endl;
        return false;
    }
    
    
    string frame = createFrame(message);
    
    size_t totalSent = 0;
    while(totalSent < frame.size()){
        ssize_t sent = send(sock, frame.data() + totalSent, frame.size() - totalSent, 0);
        //frame.data() + totalSent: Where to start sending     
        //frame.size() - totalSent: How many bytes are left
        //This is because the send() might not send all at once


        if (sent < 0 && errno == EINTR){
            continue;
        }
        if (sent <= 0){
            perror("Error sending");
            return false;
        }

        totalSent += sent;
    } 
    return true;

}

bool receiveResponse(int sock, string& response){

    response.clear();
    char header[4]; //SOH,length1,length2,STX

    int totalReceived = 0;

    while (totalReceived < 4){

        int received = recv(sock, header + totalReceived, 4 - totalReceived, 0);
        //Make sure that it works if you don't receive everything at once

        if (received < 0 && errno == EINTR){
            continue;
        }
        if (received == 0){
            cout << "Server disconnected" << endl;
            return false;
        }
        if (received < 0){
            perror("Error receiving");
            return false;
        }
        totalReceived += received; 
    }

    //Check if SOH and STX are correct
    if (header[0] != 0x01 || header[3] != 0x02){
        return false;
    }

    //Combine the 2 length bytes
    uint16_t length = ((unsigned char)header[1] << 8) | (unsigned char)header[2];


    //Check if the frame is too small or too big
    if (length < 5 || length > 5000){
        return false;
    }

    //Create a empty string for the rest of the message (we already have first 4 bytes)
    string message(length - 4, '\0');

    totalReceived = 0;
    while (totalReceived < message.size()){

        int received = recv(sock, &message[totalReceived], message.size() - totalReceived, 0);

        if (received < 0 && errno == EINTR){
            continue;
        }

        if (received <= 0){
            perror("Error receiving");
            return false;
        }

        totalReceived += received;
    }

    //Check if ETX is correct
    if (message.back() != 0x03){
        cerr << "Last byte is not ETX" << endl;
        return false;
    }

    //Remove the ETX and store only the message
    response = message.substr(0,message.size() - 1);

    return true;

}

string getTimestamp() {
    time_t now = time(nullptr);
    tm* localTime = localtime(&now);

    stringstream ss;
    ss << put_time(localTime, "%d/%m/%Y %H:%M:%S");

    return ss.str();
}

int sendUsername(int sock, string userName){
    if (userName.empty()){
        return 0;
    }

    if(!sendMessage(sock, "USERNAME," + userName)){
        return -1;
    }

    return 1;
}




bool validCommand(string command) {

    stringstream ss(command);
    string cmd, from, to, message;

    getline(ss, cmd, ',');

    if (cmd == "SENDMSG") {

        if (!getline(ss, from, ',') || from.empty()) {
            return false;
        }

        if (!getline(ss, to, ',') || to.empty()) {
            return false;
        }

        if (!getline(ss, message) || message.empty()) {
            return false;
        }

        return true;
    }

    else if (cmd == "GETMSG") {

        if (!getline(ss, from) || from.empty()) {
            return false;
        }

        return true;
    }

    else if (cmd == "STATUSREQ") {
        return command == "STATUSREQ";
    }

    return false;
}






int main(int argc, char* argv[]){
    if (argc != 3){
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        return 1;
    }
    string ip = argv[1];
    int port;
    try {
        port = stoi(argv[2]);   
    }
    catch(...){
        cerr << "Invalid port" << endl;
        return 1;
    }

    if (port < 1|| port > 65535){
        cerr << "Port has to be from 1 to 65535" << endl;
        return 1;
    }



    int serversock = connectToServer(ip, port);

    if (serversock < 0){
        return 1;
    }

    
    string userName;
    //check username
    while (true){
        cout << "Enter username: " << endl;
        if (!getline(cin, userName)){
            close(serversock);
            return 0;
        }
        int result = sendUsername(serversock, userName);

        if (result == 1){
            break;
        }
        else if (result == 0){
            cout << "Invalid Username" << endl;
        }
        else{
            cout << "Failed to send username" << endl;
            close(serversock);
            return 1;
        }
    }
    
    string command;
    string response;
    if (!receiveResponse(serversock, response)){
            cout << "Error receiving response" << endl;
            close(serversock);
            return 1;
        }
    cout << getTimestamp() << "|  Server response: " << response << endl;



    while (true){
        cout << "Enter command (Q to quit): " << endl;

        //Get teh input
        if (!getline(cin, command)){
            break;
        }

        
        if (command == "Q"){
            break;    
        }
        if (!validCommand(command)) {
            cout << "Incorrect command format" << endl;
            continue;
        }

        if (command.empty()){
            continue;
        }

        cout << getTimestamp() << "|  Command: " << command << endl;

        
        if (!sendMessage(serversock, command)){
            cout << "Error sending message" << endl;
            break;
        }

        if (!receiveResponse(serversock, response)){
            cout << "Error receiving response" << endl;
            break;
        }


         cout << getTimestamp() << "|  Server response: " << response << endl;

    }

    close(serversock);

    return 0;
}