#include "mini_iperf.h"
#include <errno.h>
#include <poll.h>
#include <sched.h>
#define MAX_PACKET_SIZE 1460// MTU-safe max size
char packet[MAX_PACKET_SIZE]; // stack buffer
static uint32_t compute_crc32(const uint8_t* data, size_t len);
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

    // Set massive send buffer
    int bufsize = 128 * 1024 * 1024;  // 128MB
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        perror("setsockopt SO_SNDBUF failed");
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(args->port + 1),
        .sin_addr.s_addr = inet_addr(args->ip_address)
    };

    // Pre-fill a batch of packets
    #define BATCH_SIZE 32
    uint8_t packets[BATCH_SIZE][MAX_PACKET_SIZE];
    int payload_size = args->packet_size - sizeof(MiniIperfHeader);
    
    // Initialize all packets in the batch
    for (int i = 0; i < BATCH_SIZE; i++) {
        MiniIperfHeader* header = (MiniIperfHeader*)packets[i];
        uint8_t* payload = packets[i] + sizeof(MiniIperfHeader);
        
        header->type = 0;
        header->flags = args->measure_delay ? 1 : 0;
        header->stream_id = htons(0);
        header->packet_size = htonl(args->packet_size);
        header->bandwidth = htonl(args->bandwidth);
        header->duration = htonl(args->duration);
        
        // Fill payload with pattern (will be updated per send)
        memset(payload, 'A', payload_size);
    }

    if (args->wait_duration > 0) sleep(args->wait_duration);

    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    uint32_t seq = 0;
    double delay_usec = (args->packet_size * 8.0 / args->bandwidth) * 1e6;

    while (1) {
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed_sec = (now_ts.tv_sec - start_ts.tv_sec) +
                           (now_ts.tv_nsec - start_ts.tv_nsec) / 1e9;

        if (args->duration > 0 && elapsed_sec >= args->duration) break;

        // Send a batch of packets
        for (int i = 0; i < BATCH_SIZE; i++) {
            MiniIperfHeader* header = (MiniIperfHeader*)packets[i];
            uint8_t* payload = packets[i] + sizeof(MiniIperfHeader);
            
            // Update sequence-specific fields
            header->sequence_number = htonl(seq + i);
            memset(payload, 'A' + ((seq + i) % 26), payload_size);
            
            // Update timestamp and CRC
            clock_gettime(CLOCK_MONOTONIC, &now_ts);
            header->timestamp_sec = htonl(now_ts.tv_sec);
            header->crc32 = 0;
            header->crc32 = htonl(compute_crc32(packets[i], args->packet_size));
        }

        // Send the entire batch
        for (int i = 0; i < BATCH_SIZE; i++) {
            ssize_t s = sendto(sock, packets[i], args->packet_size, 0,
                             (struct sockaddr*)&server_addr, sizeof(server_addr));
            if (s < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Buffer full, wait a bit
                    struct pollfd pfd = {.fd = sock, .events = POLLOUT};
                    poll(&pfd, 1, 100);
                    i--;  // Retry same packet
                    continue;
                } else {
                    perror("sendto failed");
                    close(sock);
                    return NULL;
                }
            }
        }

        seq += BATCH_SIZE;
        
        // Throttle if needed
        if (args->bandwidth > 0) {
            usleep((int)(delay_usec * BATCH_SIZE));
        }

        // Check duration after each batch
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        elapsed_sec = (now_ts.tv_sec - start_ts.tv_sec) +
                     (now_ts.tv_nsec - start_ts.tv_nsec) / 1e9;
        if (args->duration > 0 && elapsed_sec >= args->duration) break;
    }

    close(sock);
    return NULL;
}
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
    int bufsize = 128 * 1024 * 1024;  // 128MB
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    // In your receiver code, add these after socket creation:
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    int val = 1;
setsockopt(sock, SOL_SOCKET, SO_ZEROCOPY, &val, sizeof(val));

    // Enable timestamping
    setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS, &optval, sizeof(optval));

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

    // Set up poll for the socket
    struct pollfd fds;
    fds.fd = sock;
    fds.events = POLLIN;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    uint8_t packet[MAX_PACKET_SIZE];
    uint64_t total_bytes = 0;
    uint64_t payload_bytes = 0;
    uint32_t received_packets = 0;
    uint32_t corrupt_packets = 0;
    uint32_t duplicate_or_out_of_order = 0;

    uint32_t expected_seq = 0;
    uint32_t lost_packets = 0;

    struct timespec first_ts = {0}, last_ts = {0};

    while (1) {
        int poll_res = poll(&fds, 1, 100); // 100ms timeout

        if (poll_res < 0) {
            perror("poll failed");
            break;
        } else if (poll_res == 0) {
            // No data ready
            if (args->duration > 0 && received_packets > 0) {
                struct timespec now_ts;
                clock_gettime(CLOCK_MONOTONIC, &now_ts);
                double elapsed = (now_ts.tv_sec - first_ts.tv_sec) +
                                 (now_ts.tv_nsec - first_ts.tv_nsec) / 1e9;
                if (elapsed >= args->duration) break;
            }
            continue;
        }

        ssize_t bytes = recvfrom(sock, packet, args->packet_size, 0,
                                 (struct sockaddr*)&client_addr, &client_addr_len);
        if (bytes <= 0) break;

        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);

        MiniIperfHeader* header = (MiniIperfHeader*)packet;

        // Verify CRC
        uint32_t received_crc = ntohl(header->crc32);
        header->crc32 = 0;
        uint32_t computed_crc = compute_crc32(packet, args->packet_size);
        if (received_crc != computed_crc) {
            corrupt_packets++;
            continue;
        }

        uint32_t seq = ntohl(header->sequence_number);

        if (received_packets == 0) {
            first_ts = now_ts;  // Start timing after first valid packet
        }
        last_ts = now_ts;

        // Track loss and order
        if (seq == expected_seq) {
            expected_seq++;
        } else if (seq > expected_seq) {
            lost_packets += (seq - expected_seq);
            expected_seq = seq + 1;
        } else {
            duplicate_or_out_of_order++;
            lost_packets++; // treat as loss
        }

        total_bytes += bytes;
        payload_bytes += (bytes - sizeof(MiniIperfHeader));
        received_packets++;

        if (args->duration > 0) {
            double elapsed_sec = (now_ts.tv_sec - first_ts.tv_sec) +
                                 (now_ts.tv_nsec - first_ts.tv_nsec) / 1e9;
            if (elapsed_sec >= args->duration) break;
        }
    }

    double duration_sec = (last_ts.tv_sec - first_ts.tv_sec) +
                          (last_ts.tv_nsec - first_ts.tv_nsec) / 1e9;
    if (duration_sec <= 0) duration_sec = 0.001; // avoid divide-by-zero

    double throughput_mbps = (total_bytes * 8.0) / (duration_sec * 1e6);
    double goodput_mbps = (payload_bytes * 8.0) / (duration_sec * 1e6);
    double loss_pct = (expected_seq > 0) ? (double)lost_packets * 100.0 / expected_seq : 0.0;

    printf("\n=== UDP Statistics ===\n");
    printf("Duration:           %.3f sec\n", duration_sec);
    printf("Total Bytes:        %.2f MB\n", total_bytes / 1e6);
    printf("Payload Bytes:      %.2f MB\n", payload_bytes / 1e6);
    printf("Received Packets:   %u\n", received_packets);
    printf("Corrupt Packets:    %u\n", corrupt_packets);
    printf("Out-of-order/Dupes: %u\n", duplicate_or_out_of_order);
    printf("Lost Packets:       %u (%.2f%%)\n", lost_packets, loss_pct);
    printf("Throughput:         %.2f Mbps\n", throughput_mbps);
    printf("Goodput:            %.2f Mbps\n", goodput_mbps);
    printf("Start Time:         %ld.%09ld\n", first_ts.tv_sec, first_ts.tv_nsec);
    printf("End Time:           %ld.%09ld\n", last_ts.tv_sec, last_ts.tv_nsec);
    printf("Last Sequence #:    %u\n", expected_seq - 1);
    printf("============================\n");

    close(sock);
    return NULL;
}


static uint32_t compute_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = ~0U;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}
