/**
 * mini_iperf.c - Network measurement tool argument parser
 * 
 * Implements command line argument parsing for a network measurement tool
 * following the specifications from HY435 assignment.
 */

 #include "mini_iperf.h"

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

 /**
  * Main program entry point
  */
 int main(int argc, char* argv[]) {
     struct arguments args;

     if (parse_arguments(argc, argv, &args) != 0) {
         free_arguments(&args);
         return 1;
     }
     print_arguments(&args);
     free_arguments(&args);
     return 0;
 }