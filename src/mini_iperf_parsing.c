#include "mini_iperf.h"

extern pthread_mutex_t lock;
extern int line;
void safe_print(char* message) {
    pthread_mutex_lock(&lock);
    printf("%d. Buffer: %s\n", line++,message);
    pthread_mutex_unlock(&lock);
}

void init_arguments(struct arguments* args) {
    memset(args, 0, sizeof(struct arguments)); // Clear entire structure
    args->interval = 1;          // Default 1 second interval
    args->packet_size =1460;    // Default 1KB packet size
    args->num_streams = 1;       // Default single stream
    args->duration = -1;         // -1 means unlimited duration
    args->port = 5201;             // 5201 default port
    args->bandwidth = 0;        // Default no bandwidth limit
    args->wait_duration = 0;    // Default no wait duration
    // All other fields are initialized to 0/NULL by memset
}



 void free_arguments(struct arguments* args) {
    if (args->ip_address) {
        free(args->ip_address);
        args->ip_address = NULL;
    }
    if (args->filename) {
        free(args->filename);
        args->filename = NULL;
    }
}


 int is_valid_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1;
}


 int parse_arguments(int argc, char* argv[], struct arguments* args) {
    int opt;
    init_arguments(args);

    // Parse each command line option
    while ((opt = getopt(argc, argv, "a:p:i:f:scl:b:n:t:dw:h")) != -1) {
        switch (opt) {
           case 'a':  // IP address
               if (args->ip_address) free(args->ip_address);
               if (!is_valid_ip(optarg)) {
                   fprintf(stderr, "Error: Invalid IP address\n");
                   return -1;
               }
               args->ip_address = strdup(optarg);
               if (!args->ip_address) {
                   perror("Error: Memory allocation failed for IP address");
                   return -1;
               }
               break;


            case 'p':  // Port number
                args->port = atoi(optarg);
                if (args->port <= 0 || args->port > 65535) {
                    fprintf(stderr, "Error: Port must be between 1 and 65535\n");
                    return -1;
                }
                break;

            case 'i':  // Interval
                args->interval = atoi(optarg);
                if (args->interval <= 0) {
                    fprintf(stderr, "Error: Interval must be positive\n");
                    return -1;
                }
                break;

            case 'f':  // Filename
                args->filename = strdup(optarg);
                if (!args->filename) {
                    perror("Error: Memory allocation failed for filename");
                    return -1;
                }
                break;

            case 's':  // Server mode
                args->is_server = 1;
                break;

            case 'c':  // Client mode
                args->is_client = 1;
                break;

            case 'l':  // Packet size
                args->packet_size = atoi(optarg);
                if (args->packet_size <= 0) {
                    fprintf(stderr, "Error: Packet size must be positive\n");
                    return -1;
                }
                break;

            case 'b':  // Bandwidth
                args->bandwidth = atol(optarg);
                if (args->bandwidth < 0) {
                    fprintf(stderr, "Error: Bandwidth cannot be negative\n");
                    return -1;
                }
                break;

            case 'n':  // Number of streams
                args->num_streams = atoi(optarg);
                if (args->num_streams <= 0) {
                    fprintf(stderr, "Error: Number of streams must be positive\n");
                    return -1;
                }
                break;

            case 't':  // Duration
                args->duration = atoi(optarg);
                if (args->duration <= 0) {
                    fprintf(stderr, "Error: Duration must be positive\n");
                    return -1;
                }
                break;

            case 'd':  // Measure delay
                args->measure_delay = 1;
                break;

            case 'w':  // Wait duration
                args->wait_duration = atoi(optarg);
                if (args->wait_duration < 0) {
                    fprintf(stderr, "Error: Wait duration cannot be negative\n");
                    return -1;
                }
                break;

            case 'h':  // Help
            default:
                print_help();
                free_arguments(args);
                exit(opt == 'h' ? 0 : 1);
        }
    }

    // Validate argument combinations after parsing
    if (args->is_server && args->is_client) {
        fprintf(stderr, "Error: Cannot run as both server and client\n");
        return -1;
    }

    if (!args->is_server && !args->is_client) {
        fprintf(stderr, "Error: Must specify either server (-s) or client (-c) mode\n");
        return -1;
    }

    if (args->port == -1) {
        fprintf(stderr, "Error: Port number (-p) is required\n");
        return -1;
    }

    if (args->is_client) {
        if (!args->ip_address) {
            fprintf(stderr, "Error: Server address (-a) is required in client mode\n");
            return -1;
        }
        
    }

    return 0;
}

void print_arguments(const struct arguments* args) {
    printf("\n=== Parsed Arguments ===\n");
    
    // Mode and network settings
    printf("Mode:               %s\n", args->is_server ? "SERVER" : 
                                      (args->is_client ? "CLIENT" : "NONE"));
    printf("IP Address:         %s\n", args->ip_address ? args->ip_address : "(null)");
    printf("Port:               %d\n", args->port);
    printf("Output File:        %s\n", args->filename ? args->filename : "(none)");
    
    // Timing parameters
    printf("Update Interval:    %d seconds\n", args->interval);
    printf("Wait Duration:      %d seconds\n", args->wait_duration);
    
    if (args->is_client) {
        // Client-specific parameters
        printf("\nClient Configuration:\n");
        printf("Packet Size:        %d bytes\n", args->packet_size);
        printf("Bandwidth:          %ld bps\n", args->bandwidth);
        printf("Number of Streams:  %d\n", args->num_streams);
        printf("Duration:           %s\n", 
               args->duration == -1 ? "unlimited" : 
               args->duration == 0 ? "invalid (0)" : 
               args->duration > 0 ? "valid" : "invalid");
        printf("Duration Value:     %d seconds\n", args->duration);
        printf("Measurement Type:   %s\n", 
               args->measure_delay ? "One-way delay" : "Throughput");
    }
    
    printf("========================\n\n");
}

 /**
  * Print help message with usage instructions
  */
 void print_help() {
    printf("Mini-Iperf - Network Measurement Tool\n\n");
    printf("Usage: mini_iperf [options]\n\n");
    printf("Common options:\n");
    printf("  -a <address>    Server: bind address, Client: server address\n");
    printf("  -p <port>       Server: listening port, Client: server port (required)\n");
    printf("  -i <seconds>    Interval for progress updates (default: 1)\n");
    printf("  -f <filename>   Output file for results\n");
    printf("  -h              Show this help message\n\n");
    printf("Server mode (requires -s):\n");
    printf("  -s              Run in server mode\n\n");
    printf("Client mode (requires -c):\n");
    printf("  -c              Run in client mode\n");
    printf("  -l <bytes>      UDP packet size (default: 1024)\n");
    printf("  -b <bps>        Bandwidth in bits per second (required for throughput)\n");
    printf("  -n <number>     Number of parallel streams (default: 1)\n");
    printf("  -t <seconds>    Experiment duration (default: unlimited)\n");
    printf("  -d              Measure one-way delay instead of throughput\n");
    printf("  -w <seconds>    Wait time before transmission (default: 0)\n");
}

int send_tcp_message(int sock, uint8_t msg_type, const void* payload, uint32_t payload_len) {
    tcp_header_t header = {
        .msg_type = msg_type,
        .payload_len = payload_len,
        .timestamp_ns = get_monotonic_time()
    };
    
    // Send header
    if (send(sock, &header, sizeof(header), 0) < 0) {
        perror("TCP header send failed");
        return -1;
    }
    
    // Send payload if exists
    if (payload_len > 0 && payload != NULL) {
        if (send(sock, payload, payload_len, 0) < 0) {
            perror("TCP payload send failed");
            return -1;
        }
    }
    
    return 0;
}