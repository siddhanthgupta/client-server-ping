/*
 * client_socket.c
 *
 *  Created on: 05-Oct-2015
 *      Author: siddhanthgupta
 */
#include <stdio.h>              // Apparently, stderr is in stdio.h

#include <sys/socket.h>         // For socket descriptor
#include <sys/errno.h>          // For errno on error
#include <sys/types.h>

#include <netinet/in.h>         // For struct sockaddr_in, htonl, htons functions
#include <unistd.h>             // For read/write functions
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>          // For the htonl function
#include <math.h>
#include <time.h>
#include <errno.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SERVER_PORT_NO 5650
#define MAX_CONCURRENT_CLIENTS 5
#define MAX_BUFFER_SIZE 255
#define PROBABILITY_DROP 40
#define MEAN_DELAY 500000000
#define DELAY_VARIANCE 50000000

struct ping_packet {
    int seq_no;
    char message[255];
    struct timeval timestamp;
};

void display(struct ping_packet* buffer, struct sockaddr_in* from_addr) {
    char s[MAX_BUFFER_SIZE];
    if (inet_ntop(AF_INET, &(from_addr->sin_addr), s, MAX_BUFFER_SIZE) == NULL) {
        fprintf(stderr, "Unable to read address\n");
    } else {
        printf("Message received from %s: %s SEQ=%d TIME=%d\n", s,
                buffer->message, buffer->seq_no, buffer->timestamp.tv_sec);
    }
}

void usage(char* program) {
    printf("USAGE: %s <Port Number>\n", program);
}

double drand() {
    return (rand() + 1.0) / (RAND_MAX + 1.0);
}

/** Generates a random number from a normal distribution having
 *  mean = 0, variance =1. Uses a box mueller transform on two random
 *  double values from (0...1]
 */
double random_normal() {
    return sqrt(-2 * log(drand())) * cos(2 * M_PI * drand());
}

int get_gaussian_delay() {
    double delay = DELAY_VARIANCE * random_normal() + MEAN_DELAY;
    int nano_delay = (int) delay;
    if (nano_delay < 0)
        nano_delay = 0;
    return nano_delay;
}

int modified_write(int socket, void* buffer, size_t buf_size, int server_socket,
        struct sockaddr_in* client_address, int client_address_length) {
    int random_number = rand() % 100 + 1;
    if (random_number > PROBABILITY_DROP) {
        int random_delay = get_gaussian_delay();
        struct timespec timer;
        timer.tv_nsec = random_delay;
        timer.tv_sec = 0;
        nanosleep(&timer, NULL);
        write(socket, buffer, buf_size);
        sendto(server_socket, buffer, buf_size, 0,
                (struct sockaddr*) client_address, client_address_length);
        //printf("Slept for timer: %d\n", timer.tv_nsec);
        return 1;
    }
    printf("Return packet dropped\n");
    return 0;
}

/*
 * On the server side, we need to
 *  Create a socket using the socket() call
 *  Bind the socket to an address using the bind() call
 *  Listen for connections using listen() call
 *  Accept a connection using accept() system call. This causes the server to be
 *  blocked, until the 3-way TCP handshake is complete
 *
 *  To accept connections, the following steps are performed:
 *
 *      1.  A socket is created with socket(2).
 *
 *      2.  The socket is bound to a local address using bind(2),  so  that
 *          other sockets may be connect(2)ed to it.
 *
 *      3.  A  willingness to accept incoming connections and a queue limit
 *          for incoming connections are specified with listen().
 *
 *      4.  Connections are accepted with accept(2).
 *
 *
 */
int main(int argc, char** argv) {
    // Creating a socket: IPv4 domain, TCP connection oriented type
    // Protocol for this is TCP by default
    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }
//    errno = 0;
    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "%s: Error: Unable to create server socket. Exiting.\n",
                argv[0]);
        exit(1);
    }
    srand(1);
    // The socket is bound to an address using the bind() call
    // We define an address
    struct sockaddr_in address, client_address;
    int client_address_length, client_socket;
    // char buffer[MAX_BUFFER_SIZE];
    // int bytes_read;
    address.sin_family = AF_INET;
    // For the Internet domain, if we specify the special IP address INADDR_ANY (defined in
    // <netinet/in.h>), the socket endpoint will be bound to all the system’s network
    // interfaces. This means that we can receive packets from any of the network interface
    // cards installed in the system.
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(atoi(argv[1]));
    if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        fprintf(stderr, "%s: Error: Unable to bind server socket.\n", argv[0]);
        exit(1);
    }
    // We accept connections indefinitely in a loop
    while (1) {

        struct ping_packet buffer;
        int bytes_read;
        //memset(buffer, 0, MAX_BUFFER_SIZE + 1);
//        errno = 0;
        if ((bytes_read = recvfrom(server_socket, &buffer,
                sizeof(struct ping_packet), 0,
                (struct sockaddr*) &client_address,
                (int*) &client_address_length)) <= 0) {
//            fprintf(stderr, "%s : Error: No data read from client.\n", argv[0]);
//            printf("Returned %d with err_no = %s\n", bytes_read,
//                    strerror(errno));
//            exit(1);
        } else {
//            printf("%d bytes read\n", bytes_read);
            display(&buffer, &client_address);
            modified_write(server_socket, &buffer, sizeof(struct ping_packet),
                    server_socket, &client_address, client_address_length);
//            int x = sendto(server_socket, &buffer, sizeof(struct ping_packet),
//                    0, (struct sockaddr*) &client_address,
//                    client_address_length);
//            printf("Sent %d bytes. Client address length = %d, sockaddr = %d\n",
//                    x, client_address_length, sizeof(struct sockaddr_in));
        }

    }
    return 0;
}

