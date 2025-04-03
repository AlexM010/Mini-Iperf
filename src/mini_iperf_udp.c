#include "mini_iperf.h"
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <math.h>
#include <time.h>

#define MAX_PACKET_SIZE 1460  // MTU-safe max size
#define BATCH_SIZE 32
#define NS_PER_SEC 1000000000L

// Utility function to check if all bytes in buffer match expected value
static int all_bytes_equal(const void *ptr, int c, size_t n) {
    const unsigned char *p = ptr;
    while (n-- > 0) if (*p++ != c) return 0;
    return 1;
}

// Get current monotonic time in nanoseconds
static uint64_t get_monotonic_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
}

// UDP Sender Thread
void* udp_sendto(void* args_ptr) {
    struct arguments* args = (struct arguments*)args_ptr;
    if (args->packet_size > MAX_PACKET_SIZE) {
        fprintf(stderr, "Packet size too large (max %d bytes)\n", MAX_PACKET_SIZE);
        return NULL;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }
    // Optimize socket settings
    int bufsize = 128 * 1024 * 1024;  // 128MB
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    int prio = 6; // Higher priority
    setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(args->port + 1),
        .sin_addr.s_addr = inet_addr(args->ip_address)
    };

    // Calculate payload size and validate
    const int payload_size = args->packet_size - sizeof(MiniIperfHeader);
    if (payload_size <= 0) {
        fprintf(stderr, "Invalid packet size (must be > %zu)\n", sizeof(MiniIperfHeader));
        close(sock);
        return NULL;
    }

    // Pre-fill packet batch
    MiniIperfPacket* batch = malloc(BATCH_SIZE * sizeof(MiniIperfPacket));
    if (!batch) {
        perror("malloc failed");
        close(sock);
        return NULL;
    }

    for (int i = 0; i < BATCH_SIZE; i++) {
        MiniIperfHeader* header = &batch[i].header;
        header->seq_num = 0;
        header->timestamp_ns = 0;
        memset(batch[i].payload, 'A', payload_size);
    }

    if (args->wait_duration > 0) sleep(args->wait_duration);

    const uint64_t start_time = get_monotonic_time();
    uint32_t seq = 0;
    const double packet_bits = args->packet_size * 8.0;
    const double delay_ns = (packet_bits / args->bandwidth) * 1e9 * BATCH_SIZE;

    while (1) {
        const uint64_t current_time = get_monotonic_time();
        const double elapsed_sec = (current_time - start_time) / (double)NS_PER_SEC;

        // Check experiment duration
        if (args->duration > 0 && elapsed_sec >= args->duration) break;

        // Update batch with current sequence numbers and timestamps
        for (int i = 0; i < BATCH_SIZE; i++) {
            MiniIperfHeader* header = &batch[i].header;
            header->seq_num = htonl(seq + i);
            header->timestamp_ns = get_monotonic_time();
            
            // Update payload pattern
            memset(batch[i].payload, 'A' + ((seq + i) % 26), payload_size);
        }

        // Send batch with error handling
        for (int i = 0; i < BATCH_SIZE; i++) {
            ssize_t sent = sendto(sock, &batch[i], args->packet_size, 0,
                                 (struct sockaddr*)&server_addr, sizeof(server_addr));
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    struct pollfd pfd = {.fd = sock, .events = POLLOUT};
                    poll(&pfd, 1, 300);
                    i--;  // Retry same packet
                    continue;
                }
                perror("UDP sendto failed");
                free(batch);
                close(sock);
                return NULL;
            }
        }
        seq += BATCH_SIZE;

        // Throttle if bandwidth limited
        if (args->bandwidth > 0) {
            const uint64_t target_time = start_time + (uint64_t)((seq * packet_bits / args->bandwidth) * 1e9);
            const uint64_t now = get_monotonic_time();
            if (target_time > now) {
                struct timespec delay = {
                    .tv_sec = (target_time - now) / NS_PER_SEC,
                    .tv_nsec = (target_time - now) % NS_PER_SEC
                };
                nanosleep(&delay, NULL);
            }
        }
    }

    free(batch);
    close(sock);
    return NULL;
}

// UDP Receiver Thread
void* udp_recv(void* args_ptr) {
    struct arguments* args = (struct arguments*)args_ptr;
    if (args->packet_size > MAX_PACKET_SIZE) {
        fprintf(stderr, "Packet size too large\n");
        return NULL;
    }
 
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }
    
    // Optimize socket settings
    int bufsize = 256 * 1024 * 1024;  // 256MB
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    int prio = 6; // Higher priority
    setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(args->port + 1),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP bind failed");
        close(sock);
        return NULL;
    }

    // Statistics tracking with improved jitter measurement
    struct {
        uint64_t total_bytes;
        uint64_t payload_bytes;
        uint32_t received_packets;
        uint32_t corrupt_packets;
        uint32_t out_of_order;
        uint32_t lost_packets;
        uint32_t expected_seq;
        uint64_t first_ts;
        uint64_t last_ts;
        
        // Jitter calculation variables
        uint64_t last_arrival_ns;       // Last packet arrival time
        double *jitter_samples;         // Array to store inter-arrival differences
        int jitter_samples_count;       // Number of samples collected
        int jitter_samples_capacity;    // Size of jitter_samples array
        double jitter_sum;              // Sum of all jitter values
        double jitter_sum_squares;      // Sum of squares for stddev calculation
    } stats = {0};

    // Initialize jitter samples array
    stats.jitter_samples_capacity = 100000; // Adjust based on expected packet count
    stats.jitter_samples = malloc(stats.jitter_samples_capacity * sizeof(double));
    if (!stats.jitter_samples) {
        perror("Failed to allocate jitter tracking array");
        close(sock);
        return NULL;
    }

    MiniIperfPacket packet;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        struct pollfd pfd = {.fd = sock, .events = POLLIN};
        int ready = poll(&pfd, 1, 10);  // 10ms timeout
        
        if (ready < 0) {
            perror("poll failed");
            break;
        } else if (ready == 0) {
            // Timeout - check if we should exit
            if (args->duration > 0 && stats.received_packets > 0) {
                const double elapsed = (get_monotonic_time() - stats.first_ts) / (double)NS_PER_SEC;
                if (elapsed >= args->duration) break;
            }
            continue;
        }

        // Receive packet
        ssize_t bytes = recvfrom(sock, &packet, sizeof(packet), 0,
                               (struct sockaddr*)&client_addr, &addr_len);
        if (bytes <= 0) break;

        const uint64_t recv_time = get_monotonic_time();

        // Validate packet
        if (bytes < sizeof(MiniIperfHeader)) {
            stats.corrupt_packets++;
            continue;
        }

        const uint32_t seq = ntohl(packet.header.seq_num);
        const int payload_size = bytes - sizeof(MiniIperfHeader);
        const uint8_t expected_char = 'A' + (seq % 26);

        // Replace the full payload check with sampling
        if (payload_size > 64) {
            // Only check first/last 8 bytes to reduce CPU load
            if (!all_bytes_equal(packet.payload, expected_char, 8) || 
                !all_bytes_equal(packet.payload + payload_size - 8, expected_char, 8)) {
                stats.corrupt_packets++;
                continue;
            }
        } else {
            if (!all_bytes_equal(packet.payload, expected_char, payload_size)) {
                stats.corrupt_packets++;
                continue;
            }
        }

        // Update sequence tracking
        if (stats.received_packets == 0) {
            stats.first_ts = recv_time;
            stats.expected_seq = seq + 1;
            stats.last_arrival_ns = recv_time;
        } else {
            // Calculate and store inter-arrival time (jitter)
            uint64_t delta_ns = recv_time - stats.last_arrival_ns;
            double delta_ms = delta_ns / 1e6; // Convert to milliseconds
            
            // Store jitter sample if we have space
            if (stats.jitter_samples_count < stats.jitter_samples_capacity) {
                stats.jitter_samples[stats.jitter_samples_count++] = delta_ms;
                stats.jitter_sum += delta_ms;
                stats.jitter_sum_squares += delta_ms * delta_ms;
            }
            
            // Update for next calculation
            stats.last_arrival_ns = recv_time;
            
            // Handle sequence numbers
            if (seq == stats.expected_seq) {
                stats.expected_seq++;
            } else if (seq > stats.expected_seq) {
                stats.lost_packets += (seq - stats.expected_seq);
                stats.expected_seq = seq + 1;
            } else {
                stats.out_of_order++;
            }
        }

        // Update statistics
        stats.last_ts = recv_time;
        stats.total_bytes += bytes;
        stats.payload_bytes += payload_size;
        stats.received_packets++;

        // Check experiment duration
        if (args->duration > 0) {
            const double elapsed = (recv_time - stats.first_ts) / (double)NS_PER_SEC;
            if (elapsed >= args->duration) break;
        }
    }

    // Calculate final statistics
    double duration_sec = (stats.last_ts - stats.first_ts) / (double)NS_PER_SEC;
    if (duration_sec <= 0) duration_sec = 1e-9;

    printf("\n=== UDP Statistics ===\n");
    printf("Duration:        %.3f sec\n", duration_sec);
    printf("Total Bytes:     %.2f MB\n", stats.total_bytes / 1e6);
    printf("Payload Bytes:   %.2f MB\n", stats.payload_bytes / 1e6);
    printf("Valid Packets:   %u\n", stats.received_packets);
    printf("Corrupt Packets: %u\n", stats.corrupt_packets);
    printf("Out-of-Order:    %u\n", stats.out_of_order);
    printf("Lost Packets:    %u (%.2f%%)\n", stats.lost_packets,
           stats.expected_seq > 0 ? 100.0 * stats.lost_packets / stats.expected_seq : 0.0);
    printf("Throughput:      %.2f Mbps\n", (stats.total_bytes * 8.0) / (duration_sec * 1e6));
    printf("Goodput:         %.2f Mbps\n", (stats.payload_bytes * 8.0) / (duration_sec * 1e6));
    
    // Calculate jitter statistics
    if (stats.jitter_samples_count > 1) {
        double mean_jitter = stats.jitter_sum / stats.jitter_samples_count;
        
        // Calculate standard deviation
        double variance = (stats.jitter_sum_squares / stats.jitter_samples_count) - 
                         (mean_jitter * mean_jitter);
        double stddev = sqrt(variance > 0 ? variance : 0);
        
        printf("Avg Jitter:      %.3f ms\n", mean_jitter);
        printf("Jitter Std Dev:  %.3f ms\n", stddev);
    }
    
    printf("========================\n");

    // Clean up
    free(stats.jitter_samples);
    close(sock);
    return NULL;
}