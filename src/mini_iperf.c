/**
 * mini_iperf.c - Network measurement tool argument parser
 *
 * Implements command line argument parsing for a network measurement tool
 * following the specifications from HY435 assignment.
 */

#include "mini_iperf.h"


/* Extern variables */
extern int client_socket;
extern int server_socket;
/* Definitions */
#define  SERVER 1
#define  CLIENT 0  
#define  UNDEFINED -1  

/* Global variables */
int who = UNDEFINED;
int line=0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t server_recv_thread, server_send_thread,client_send_thread,client_recv_thread;
pthread_t udp_sender_thread, udp_receiver_thread;
volatile sig_atomic_t stop_flag = 1;
/**
 * Signal handler for SIGINT (Ctrl+C)
 * @param sig Signal number
 */

 void sigint_handler(int sig) {
    char c;
    signal(sig, SIG_IGN); // Ignore further SIGINT while handling
    
    printf("\nDo you really want to terminate the experiment? (y/n): ");
    fflush(stdout);
    
    // Clear input buffer
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
    
    c = getchar();
    if (c == 'y' || c == 'Y') {
        printf("Terminating experiment...\n");
        stop_flag = 0;
        if (who == SERVER && server_socket > 0) {
            // Server-specific cleanup
            send_tcp_message(server_socket, MSG_STOP_EXP, NULL, 0);
            server_close(server_socket);
        } 
        else if (who == CLIENT && client_socket > 0) {
            // Client-specific cleanup
            send_tcp_message(client_socket, MSG_STOP_EXP, NULL, 0);
            client_close(client_socket);
        }
    } else {
        printf("Continuing experiment...\n");
        signal(SIGINT, sigint_handler); // Re-enable signal handler
    }
}

int duration;
struct arguments args;
 
 int main(int argc, char* argv[]) {
    signal(SIGINT, sigint_handler);

     if (parse_arguments(argc, argv, &args) != 0) {
        free_arguments(&args);
        return 1;
     }
    if (args.is_server && args.is_client) {
        fprintf(stderr, "Error: Cannot run as both server and client\n");
        free_arguments(&args);
        return 1;
    }
     duration = args.duration;


    if (args.is_server) {
        server_socket = server_start(args.ip_address,args.port);
        if (server_socket < 0) {
            free_arguments(&args);
            return 1;
        }
        who = SERVER;
        int client_socket = server_accept(server_socket);
        if (client_socket < 0) {
            server_close(server_socket);
            free_arguments(&args);
            return 1;
        }
        pthread_create(&server_recv_thread, NULL, server_channel_recv, (void*)&client_socket);
        
        pthread_join(server_recv_thread, NULL);
    } else if (args.is_client) {
        client_socket = client_connect(args.ip_address, args.port);
        if (client_socket < 0) {
            free_arguments(&args);
            return 1;
        }
        who = CLIENT;
        pthread_create(&client_send_thread, NULL, client_channel_send, (void*)&client_socket);
        
        pthread_join(client_send_thread, NULL);
    }

    // print_arguments(&args);
     free_arguments(&args);
     return 0;
 }