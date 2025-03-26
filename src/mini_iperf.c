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

void sigint_handler(int sig) {
    char c;
    printf("\nDo you want to exit? (y/n): ");
    c = getchar();
    if (c == 'y' || c == 'Y') {
        printf("Exiting...\n");
        if(who == SERVER){
            close(server_socket);
            close(client_socket);
        }
        else if(who == CLIENT)
            close(client_socket);

        exit(0);
    }
}

 /**
  * Main program entry point
  */
 int main(int argc, char* argv[]) {
    struct arguments args;
    signal(SIGINT, sigint_handler);

     if (parse_arguments(argc, argv, &args) != 0) {
         free_arguments(&args);
         return 1;
     }


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
        pthread_t server_recv_thread, server_send_thread;
        pthread_create(&server_recv_thread, NULL, server_channel_recv, (void*)&client_socket);
        pthread_create(&server_send_thread, NULL, server_channel_send, (void*)&client_socket);
        pthread_join(server_recv_thread, NULL);
        pthread_join(server_send_thread, NULL);
    } else if (args.is_client) {
        client_socket = client_connect(args.ip_address, args.port);
        if (client_socket < 0) {
            free_arguments(&args);
            return 1;
        }
        who = CLIENT;
        pthread_t client_send_thread,client_recv_thread;
        pthread_create(&client_recv_thread, NULL, client_channel_recv, (void*)&client_socket);
        pthread_create(&client_send_thread, NULL, client_channel_send, (void*)&client_socket);
        pthread_join(client_recv_thread, NULL);
        pthread_join(client_send_thread, NULL);
    }

     print_arguments(&args);
     free_arguments(&args);
     return 0;
 }