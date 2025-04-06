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
extern int duration;
extern pthread_t udp_sender_thread;
extern pthread_t client_recv_thread;
extern volatile sig_atomic_t stop_flag;
extern struct arguments args;

extern int64_t clock_offset;
/**
 * @brief Connect to the server
 * @param server_ip IP address of the server
 * @param server_port Port number of the server
 * @return Socket file descriptor on success, -1 on error
 */
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
    shutdown(client_socket, SHUT_RDWR);
    // Close the client socket
    if (close(client_socket) < 0) {
        perror("Error: Socket close failed");
        return -1;
    }
    printf("Connection closed\n");

    return 0;
}

void* client_channel_recv(void* client_socket) {
    int sock = *(int*)client_socket;
    tcp_header_t header;
    int64_t clock_offset = 0;
    printf("HEYYYYYY\n");
    while (1) {
        if (recv(sock, &header, sizeof(header), MSG_WAITALL) <= 0) {
            break; // Server disconnected
        }

        switch (header.msg_type) {
            case MSG_SYNC_RESP: {
                // Finalize clock sync
                uint64_t t3 = get_monotonic_time();
                uint64_t t1;
                recv(sock, &t1, sizeof(t1), MSG_WAITALL);
                clock_offset = ((header.timestamp_ns - t1) + (t3 - header.timestamp_ns)) / 2;
                break;
            }
            
            case MSG_STATS: {
                experiment_stats_t stats;
                recv(sock, &stats, sizeof(stats), MSG_WAITALL);
                // Process statistics
                break;
            }
            
            case MSG_ACK: {
                // Acknowledgment received
                break;
            }
            case MSG_STOP_EXP: {
                // Stop experiment command received
                printf("Experiment stopped by server\n");
                stop_flag=0;
                break;
            }
            
            default:
                printf("Unknown message type: %d\n", header.msg_type);
        }
    }
    
    printf("Server disconnected\n");
    return NULL;
}
void* client_channel_send(void* client_socket) {
    int sock = *(int*)client_socket;
    // 1. Perform clock synchronization
    uint64_t t1 = get_monotonic_time();
    if (send_tcp_message(sock, MSG_SYNC, &t1, sizeof(t1)) < 0) {
        perror("Failed to send sync request");
        return NULL;
    }

    // Wait for server's response
    tcp_header_t header;
    if (recv(sock, &header, sizeof(header), MSG_WAITALL) <= 0) {
        perror("Failed to receive sync header");
        return NULL;
    }

    if (header.msg_type == MSG_SYNC_RESP) {
        uint64_t sync_data[2];
        if (recv(sock, &sync_data, sizeof(sync_data), MSG_WAITALL) <= 0) {
            perror("Failed to receive sync data");
            return NULL;
        }
        
        uint64_t t1 = sync_data[0];  // Original client time
        uint64_t t2 = sync_data[1];  // Server receive time
        uint64_t t3 = get_monotonic_time(); // Client receive time
        
        // Calculate clock offset (NTP-style)
        clock_offset = ((int64_t)(t2 - t1) - (int64_t)(t3 - t2)) / 2;
        printf("Clock synchronized. Offset: %ld ns\n", clock_offset);
    } else {
        fprintf(stderr, "Unexpected message type during sync: %d\n", header.msg_type);
        return NULL;
    }
    // 2. Send experiment start command
    send_tcp_message(sock, MSG_START_EXP,NULL, 0);
    pthread_create(&client_recv_thread, NULL, client_channel_recv, (void*)&sock);
    if(args.wait_duration > 0) {
        sleep(args.wait_duration); // Wait for the specified duration
    }
    pthread_create(&udp_sender_thread, NULL, udp_sendto, (void*)&args);
    if (duration>0)
    {
        sleep(duration); // Wait for the experiment duration
        send_tcp_message(sock, MSG_STOP_EXP, NULL, 0); // Send stop command
        stop_flag = 0; // Set stop flag to terminate threads
    }
    //thread to create  client channel recv trread

    pthread_join(client_recv_thread, NULL);
    pthread_join(udp_sender_thread, NULL);
    return NULL;
}