#include "mini_iperf.h"

void init_arguments(struct arguments* args) {
    memset(args, 0, sizeof(struct arguments)); // Clear entire structure
    args->interval = 1;          // Default 1 second interval
    args->packet_size = 1024;    // Default 1KB packet size
    args->num_streams = 1;       // Default single stream
    args->duration = -1;         // -1 means unlimited duration
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
        if (!args->measure_delay && args->bandwidth <= 0) {
            fprintf(stderr, "Error: Bandwidth (-b) is required for throughput measurement\n");
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