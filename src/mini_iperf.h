/**
 * @file mini_iperf.h
 * @brief Header file for the Mini-Iperf project.
 *
 * This file contains the declarations for the Mini-Iperf application, 
 * which is a lightweight implementation of a network performance 
 * measurement tool. It provides functionality for both server and 
 * client modes to measure network bandwidth and performance.
 *
 * Sections:
 * - Initial Functions: Declarations for initialization and setup.
 * - Server Functions: Declarations for server-side operations.
 * - Client Functions: Declarations for client-side operations.
 *
 * @note Ensure proper implementation of the corresponding functions 
 *       in the source file to maintain functionality.
 */

#ifndef MINI_IPERF_H
#define MINI_IPERF_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>

/* Initial Functions and Structures */

/**
  * Structure to hold all command line parameters
  */
 struct arguments {
    char* ip_address;       // -a: IP address (bind address for server, server address for client)
    int port;               // -p: Port number
    int interval;           // -i: Progress update interval in seconds
    char* filename;         // -f: Output filename for results
    int is_server;          // -s: Flag for server mode
    int is_client;          // -c: Flag for client mode
    int packet_size;        // -l: UDP packet size in bytes
    long bandwidth;         // -b: Bandwidth in bits per second
    int num_streams;        // -n: Number of parallel streams
    int duration;           // -t: Experiment duration in seconds
    int measure_delay;      // -d: Flag for delay measurement mode
    int wait_duration;      // -w: Wait duration before transmission
};
#define HEADER_SIZE 16  // Define fixed header size (adjust as needed)
/**
 * Structure to represent a data packet
 */
typedef struct {
  MiniIperfHeader header; // Custom header for Mini-Iperf
  char payload[];         // Flexible array member for payload data
} MiniIperfPacket;
/**
 * Structure to represent the custom header for Mini-Iperf
 */
typedef struct {
    uint16_t source_port;   // 2 bytes
    uint16_t dest_port;     // 2 bytes
    uint32_t seq_number;    // 4 bytes
    uint32_t ack_number;    // 4 bytes
    uint16_t flags;         // 2 bytes (optional for control flags)
    uint16_t checksum;      // 2 bytes (for error detection)
} MiniIperfHeader;

 /**
  * Initialize arguments structure with default values
  * @param args Pointer to arguments structure to initialize
  */
void init_arguments(struct arguments* args);

 /**
  * Free dynamically allocated memory in arguments structure
  * @param args Pointer to arguments structure to clean up
  */
void free_arguments(struct arguments* args);

/**
  * Validate an IP address string
  * @param ip IP address string to validate
  * @return 1 if valid, 0 if invalid
  */
int is_valid_ip(const char* ip);

/**
  * Parse command line arguments
  * @param argc Argument count from main()
  * @param argv Argument vector from main()
  * @param args Pointer to arguments structure to populate
  * @return 0 on success, -1 on error
  */
int parse_arguments(int argc, char* argv[], struct arguments* args);
/**
 * Print all parsed arguments for debugging
 * @param args Pointer to arguments structure
 */
void print_arguments(const struct arguments* args);

/**
 * Print help message with usage instructions
 */
void print_help();

void safe_print(char* message);


// TCP Channel Functions
//Server Functions
int server_start(const char* ip, int port);
int server_accept(int server_socket);
int server_receive(int client_socket, char* buffer, int buffer_size);
int server_send(int client_socket, const char* message, int message_size);
int server_close(int server_socket);
void * server_channel_send(void* client_socket);
void* server_channel_recv(void* client_socket);
//Client Functions
int client_connect(const char* server_ip, int server_port);
int client_send(int client_socket, const char* message, int message_size);
int client_receive(int client_socket, char* buffer, int buffer_size);
int client_close(int client_socket);
void* client_channel_send(void* client_socket);
void* client_channel_recv(void* client_socket);




#endif