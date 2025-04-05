/*
 * mini_iperf_server.c
 * 
 * This file is part of the Mini-Iperf project, which is a lightweight implementation
 * of a client-server model for testing network performance.
 * 
 * This file implements the server-side functionality. The server listens on a specified
 * port for incoming client connections, receives data from the client,
 * and prints the received data to the console.
 * 
 * The corresponding client implementation can be found in the `mini_iperf_client.c` file.
 * Together, these files demonstrate basic socket programming in C.
 */

#include "mini_iperf.h"

int server_socket=-1;
extern volatile sig_atomic_t stop_flag;
extern pthread_t udp_receiver_thread;
extern struct arguments args;

int server_start(const char* ip, int port) {
    // Create a socket for the server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error: Socket creation failed");
        return -1;
    }

    // Define the server address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (ip == NULL) {
        server_address.sin_addr.s_addr = INADDR_ANY;
    } else {
        server_address.sin_addr.s_addr = inet_addr(ip);
    }
    // Bind the socket to the server address
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Error: Bind failed");
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("Error: Listen failed");
        return -1;
    }

    printf("Server started successfully.\n");
    printf("Listening on IP: %s, Port: %d\n", ip == NULL ? "0.0.0.0" : ip, port);


    return server_socket;
}

int server_accept(int server_socket) {
    // Accept a client connection
    int client_socket;
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_len);
    if (client_socket < 0) {
        perror("Error: Accept failed");
        return -1;
    }


    return client_socket;
}

int server_receive(int client_socket, char* buffer, int buffer_size) {
    // Receive data from the client
    int bytes_received = recv(client_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Error: Receive failed");
        return -1;
    }

    return bytes_received;
}

int server_send(int client_socket, const char* message, int message_size) {
    // Send the message to the client
    int bytes_sent = send(client_socket, message, message_size, 0);
    if (bytes_sent < 0) {
        perror("Error: Send failed");
        return -1;
    }
    printf("Sent %d bytes to the client.\n", bytes_sent);

    return bytes_sent;
}

int server_close(int server_socket) {
    // Close the server socket
    if (close(server_socket) < 0) {
        perror("Error: Socket close failed");
        return -1;
    }
    printf("Server socket closed successfully.\n");
    return 0;
}


void* server_channel_recv(void* client_socket) {
    int sock = *(int*)client_socket;
    tcp_header_t header;
    int64_t clock_offset = 0;

    while (1) {
        // Receive header
        if (recv(sock, &header, sizeof(header), MSG_WAITALL) <= 0) {
            break; // Client disconnected
        }

        switch (header.msg_type) {
            case MSG_SYNC: {
                // Clock synchronization
                uint64_t t2 = get_monotonic_time();
                send_tcp_message(sock, MSG_SYNC_RESP, &header.timestamp_ns, sizeof(uint64_t));
                break;
            }
            
            case MSG_START_EXP: {
                printf("Experiment started by client\n");
                pthread_create(&udp_receiver_thread, NULL, udp_recv, (void*)&args);
                break;
            }
            
            case MSG_STOP_EXP: {
                printf("Experiment stopped by client\n");
                stop_flag=0;
                break;
            }
            
            default:
                printf("Unknown message type: %d\n", header.msg_type);
        }
    }
    
    printf("Client disconnected\n");
    return NULL;
}

void* server_channel_send(void* client_socket) {
    // This thread can now be used for:
    // 1. Periodic statistics reporting
    // 2. Unsolicited server notifications
    // 3. Keepalive messages
    
    int sock = *(int*)client_socket;
    
    while (1) {
        sleep(1); // Example: Send periodic updates every second
        // You can add periodic reporting here if needed
    }
    
    return NULL;
}