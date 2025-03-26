/*
 * mini_iperf_server.c
 * 
 * This file is part of the Mini-Iperf project, which is a lightweight implementation
 * of a client-server model for testing network performance.
 * 
 * This file implements the client-side functionality. The client connects to a specified
 * server address and port, sends data to the server, and
 * prints the received data to the console.
 * 
 * 
 */
#include "mini_iperf.h"
int client_socket=-1;

int client_connect(const char* server_ip, int server_port){

    // Create a socket for the client
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error: Socket creation failed");
        return -1;
    }

    // Define the server address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Error: Connection failed");
        return -1;
    }
    printf("Connected to server %s on port %d\n", server_ip, server_port);

    return client_socket;
}
int client_send(int client_socket, const char* message, int message_size) {
    // Send the message to the server
    int bytes_sent = send(client_socket, message, message_size, 0);
    if (bytes_sent < 0) {
        perror("Error: Send failed");
        return -1;
    }
    printf("Sent %d bytes to the server\n", bytes_sent);
    return bytes_sent;
}
int client_receive(int client_socket, char* buffer, int buffer_size) {
    // Receive data from the server
    int bytes_received = recv(client_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Error: Receive failed");
        return -1;
    }

    return bytes_received;
}
//terminate tcp connection
int client_close(int client_socket) {
    // Close the client socket
    if (close(client_socket) < 0) {
        perror("Error: Socket close failed");
        return -1;
    }
    printf("Connection closed\n");

    return 0;
}

//read from getline and send to server
void* client_channel_send(void* client_socket){
    char* buffer = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&buffer, &len, stdin)) != -1) {
        if (send(*(int*)client_socket, buffer, read, 0) < 0) {
            free(buffer);
            client_close(* (int*)client_socket);
            return NULL;
        }
    }
    free(buffer);
    return NULL;
}
//this is a thread function client receive (client socket)
void* client_channel_recv(void* client_socket) {
    char buffer[1024];
    
    while (1) {
        int bytes_received = recv(*(int*)client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            perror("Error: Receive failed");
            break;
        } else if (bytes_received == 0) {
            printf("Connection closed by server\n");
            exit(0);
            break;
        }
        buffer[bytes_received] = '\0';
        safe_print(buffer);
    }
    return NULL;
}