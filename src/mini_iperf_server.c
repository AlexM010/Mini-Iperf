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
#define NS_PER_SEC 1000000000L
int server_socket=-1;
extern volatile sig_atomic_t stop_flag;
extern pthread_t *udp_receiver_threads;
extern struct arguments args;
uint64_t clock_offset;  // For OWD calculations
extern uint64_t current_mbps;

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

    fprintf(args.out,"Server started successfully.\n");
    fprintf(args.out,"Listening on IP: %s, Port: %d\n", ip == NULL ? "0.0.0.0" : ip, port);


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
    fprintf(args.out,"Sent %d bytes to the client.\n", bytes_sent);

    return bytes_sent;
}

int server_close(int server_socket) {
    //shutdown first
    shutdown(server_socket, SHUT_RDWR);
    // Close the server socket
    if (close(server_socket) < 0) {
        perror("Error: Socket close failed");
        return -1;
    }
    fprintf(args.out,"Server socket closed successfully.\n");
    return 0;
}


void* server_channel_recv(void* client_socket) {
    int sock = *(int*)client_socket;
    tcp_header_t header;
    clock_offset = 0;
    pthread_t server_send_thread;
    while (stop_flag) {
        // Receive header
        if (recv(sock, &header, sizeof(header), MSG_WAITALL) <= 0) {
            break; // Client disconnected
        }

        switch (header.msg_type) {
            case MSG_SYNC: {
                // Clock synchronization request from client
                uint64_t t1, t2;
                
                // Receive client's t1
                if (recv(sock, &t1, sizeof(t1), MSG_WAITALL) <= 0) {
                    perror("Failed to receive t1");
                    break;
                }
                
                // Get server's receive time (t2)
                t2 = get_monotonic_time();
                
                // Send back both t1 and t2
                uint64_t sync_data[2] = {t1, t2};
                if (send_tcp_message(sock, MSG_SYNC_RESP, sync_data, sizeof(sync_data)) < 0) {
                    perror("Failed to send sync response");
                }
                break;
            }
            case MSG_START_EXP: {
                struct arguments arg;
                recv(sock, &arg, sizeof(arg), MSG_WAITALL);
                args.measure_delay = ntohl(arg.measure_delay);
                args.num_streams=ntohl(arg.num_streams);
                fprintf(args.out,"Experiment started by client\n");
                udp_receiver_threads=malloc(sizeof(pthread_t)*args.num_streams);
                for(int i = 0; i < args.num_streams; i++) {
                    struct arguments* arg= malloc(sizeof(struct arguments));
                    memcpy(arg, &args, sizeof(struct arguments));
                    arg->stream_id = i;
                    arg->port=args.port+i+1;
                    pthread_create(&udp_receiver_threads[i], NULL, udp_recv, (void*)arg);
                }
                pthread_create(&server_send_thread, NULL, server_channel_send, (void*)&sock);
                stop_flag = 1; // Set the flag to indicate the experiment is running

                break;
            }

            case MSG_STOP_EXP: {
                fprintf(args.out,"Experiment stopped by client\n");
                stop_flag = 0;
                // Wait for UDP receiver thread to finish
                for(int i=0;i<args.num_streams;i++){
                    pthread_join(udp_receiver_threads[i], NULL);
                }
                pthread_kill(server_send_thread, 0); // Terminate the send thread
                break;
            }
            default:
                fprintf(stderr,"Unknown message type: %d\n", header.msg_type);
        }
    }

    fprintf(args.out,"Client disconnected\n");
    return NULL;
}

void* server_channel_send(void* client_socket) {
    int sock = *(int*)client_socket;
    experiment_stats_t stats = {0};
    uint64_t last_report_time = 0;
    
    while (stop_flag) {
        uint64_t current_time = get_monotonic_time();
        
        // Check if it's time to send a report (every args.interval seconds)
        if ((current_time - last_report_time) / NS_PER_SEC >= args.interval) {

            stats.current_mbps =current_mbps; // Calculate current throughput

            if (send_tcp_message(sock, MSG_INTERIM, &stats, sizeof(stats))) {
                perror("Failed to send interim statistics");
                break;
            }
            
            last_report_time = current_time;
        }
        
        // Sleep for a short time to avoid busy waiting
        sleep(args.interval); // 100ms
    }
    
    return NULL;
}