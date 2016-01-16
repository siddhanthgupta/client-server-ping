/*
 * server_socket.c
 *
 *  Created on: 05-Oct-2015
 *      Author: siddhanthgupta
 */
#include <sys/socket.h>         // For socket
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <netinet/in.h>
#include <netdb.h>              // For gethostbyname

void usage(char* program) {
    printf("USAGE: %s <Server IP> <Port Number>\n", program);
}

/*
 * The steps involved in establishing a socket on the client side are as follows:
 *
 *     1)   Create a socket with the socket() system call
 *     2)   Connect the socket to the address of the server using the connect() system call
 *     3)   Send and receive data. There are a number of ways to do this, but the simplest
 *          is to use the read() and write() system calls.
 *
 */
int main(int argc, char** argv) {
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }
    int client_socket;
    struct hostent *server;
    struct sockaddr_in server_address;
    int server_address_length;
    // Creating the socket
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "%s: Error: Unable to create socket.\n", argv[0]);
        exit(1);
    }

    // Setting server address using the IP provided by the user
    if ((server = gethostbyname(argv[1])) == NULL) {
        fprintf(stderr, "%s: Error: Invalid host.\n", argv[0]);
        exit(1);
    }
    int port_number = atoi(argv[2]);
    server_address.sin_family = AF_INET;
    memcpy((char*) &(server_address.sin_addr.s_addr),
            (char*) server->h_addr_list[0], (size_t) server->h_length);
    server_address.sin_port = htons(port_number);

    int counter = 1;
    while (1) {
        //sleep(1);
        char str[255];
        sprintf(str, "The Lannisters send their regards %d", counter);
        sendto(client_socket, str, 255, 0, (struct sockaddr*) &server_address,
                sizeof(struct sockaddr_in));
        char buffer[255];
        int bytes_read;
        memset(buffer, 0, 255);
        if ((bytes_read = recvfrom(client_socket, buffer, 255, 0,
                NULL, NULL)) <= 0) {
            fprintf(stderr, "%s : Error: No data read from client.\n", argv[0]);
        }
        else {
        printf("Received: %s\n", buffer);
        counter++;
        }

    }
    return 0;
}

