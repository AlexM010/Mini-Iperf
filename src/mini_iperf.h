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
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <math.h>


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
#define HEADER_SIZE 24  // Define fixed header size (adjust as needed)
/**
 * Structure to represent the custom header for Mini-Iperf
 */
typedef struct {
  uint8_t     msg_type;       // Message type (from enum below)
  uint32_t    payload_len;    // Length of the payload
  uint16_t    crc;            // Checksum (optional)
  uint64_t    timestamp_ns;   // For clock synchronization
  int64_t     clock_offset;   // For OWD calculations
} __attribute__((packed)) tcp_header_t;

// Enhanced message types
enum MessageType {
  MSG_SYNC = 1,        // Clock synchronization request
  MSG_SYNC_RESP = 2,   // Clock synchronization response
  MSG_START_EXP = 3,   // Start experiment command
  MSG_STOP_EXP = 4,    // Stop experiment command
  MSG_STATS = 5,       // Statistics report
  MSG_ACK = 6,         // Acknowledgment
  MSG_CONTROL = 7      // General control message
};

// Structure for experiment statistics
typedef struct {
  uint64_t total_packets;
  uint64_t lost_packets;
  double avg_owd_ms;
  double min_owd_ms;
  double max_owd_ms;
  double jitter_ms;
  double throughput_mbps;
} __attribute__((packed)) experiment_stats_t;
/**
 * Structure to represent the custom header for Mini-Iperf
 */
typedef struct {
  uint32_t    seq_num;        // Sequence number (for loss detection)
  uint64_t    timestamp_ns;   // Monotonic clock timestamp (CLOCK_MONOTONIC)
  // Optional: Add fields for OWD (if clocks are synced via NTP/PTP)
} __attribute__((packed)) MiniIperfHeader;

/**
 * Structure to represent a data packet
 */
typedef struct {
  MiniIperfHeader header; // Custom header for Mini-Iperf
  char payload[1460];         // Flexible array member for payload data
} MiniIperfPacket;

//total bytes = 16 B


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
int send_tcp_message(int sock, uint8_t msg_type, const void* payload, uint32_t payload_len);
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

// UDP Channel Functions
void *udp_sendto(void* args);
void* udp_recv(void* args);




#endif