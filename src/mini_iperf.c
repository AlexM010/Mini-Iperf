/**
 * mini_iperf.c - Network measurement tool argument parser
 *
 * Implements command line argument parsing for a network measurement tool
 * following the specifications from HY435 assignment.
 */

 #include "mini_iperf.h"

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