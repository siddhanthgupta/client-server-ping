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
#include <signal.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_CONCURRENT_CLIENTS 5
#define MAX_BUFFER_SIZE 255
#define PROBABILITY_DROP 40
#define MEAN_DELAY 500000000
#define DELAY_VARIANCE 50000000
#define TCP_PROTOCOL 1
#define UDP_PROTOCOL 2

struct ping_packet {
    int seq_no;
    char message[255];
    struct timeval timestamp;
};

int server_socket;
int protocol;
int port;
const char* str_tcp;
const char* str_udp;

void display(struct ping_packet* buffer, struct sockaddr_in* from_addr) {
    char s[MAX_BUFFER_SIZE];
    if (inet_ntop(AF_INET, &(from_addr->sin_addr), s, MAX_BUFFER_SIZE) == NULL) {
        fprintf(stderr, "Unable to read address\n");
    } else {
        printf("Message received from %s: %s SEQ=%d TIME=%lld\n", s,
                buffer->message, buffer->seq_no,
                (long long int) (buffer->timestamp.tv_sec));
    }
}

void usage(char** argv) {
    char* program = argv[0];
    printf("USAGE: %s [-t TCP/UDP] <Port Number>\n", program);
    exit(1);
}

void socket_kill() {
    if(close(server_socket)<0) {
        perror("socket_kill(): Error: No socket to close. ");
        exit(1);
    }
    exit(0);
}

void initialize_global() {
    str_tcp = "TCP";
    str_udp = "UDP";
    protocol = TCP_PROTOCOL;
    port = 0;
    atexit(socket_kill);
//    signal(SIGINT, socket_kill);
    srand(1);
}

void options(int argc, char** argv) {
    int ch;
    // Processing command line options
    while ((ch = getopt(argc, argv, ":t:")) != EOF) {

        switch (ch) {
        case 't':

            if (optarg == 0)
                usage(argv);
            char str[100];
            strcpy(str, optarg);
//            printf("%s %s %s\n", str, str_tcp, str_udp);
            if (strcasecmp(str, str_tcp) == 0)
                protocol = TCP_PROTOCOL;
            else if (strcasecmp(str, str_udp) == 0)
                protocol = UDP_PROTOCOL;
            else
                usage(argv);

            break;
        default:
            usage(argv);
        }
    }
    // optind = index of next string after last processed argument
//    printf("optind is %d\n", optind);
    argc -= optind;
    argv += optind;
    if (argc != 1)
        usage(argv);
    port = atoi(*argv);
    printf("Protocol is %s. Port is %d\n", protocol==TCP_PROTOCOL?str_tcp:str_udp, port);
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

int modified_write_tcp(int socket, void* buffer, size_t buf_size) {
    int random_number = rand() % 100 + 1;
    if (random_number > PROBABILITY_DROP) {
        int random_delay = get_gaussian_delay();
        struct timespec timer;
        timer.tv_nsec = random_delay;
        timer.tv_sec = 0;
        nanosleep(&timer, NULL);
        write(socket, buffer, buf_size);

        printf("Slept for timer: %d.%d us\n", random_delay / 1000,
                random_delay % 1000);
        return 1;
    }
    printf("Return packet dropped\n");
    return 0;
}

int modified_write_udp(void* buffer, size_t buf_size,
        int server_socket, struct sockaddr_in* client_address,
        int client_address_length) {
    int random_number = rand() % 100 + 1;
    if (random_number > PROBABILITY_DROP) {
        int random_delay = get_gaussian_delay();
        struct timespec timer;
        timer.tv_nsec = random_delay;
        timer.tv_sec = 0;
        nanosleep(&timer, NULL);
//        write(socket, buffer, buf_size);
        sendto(server_socket, buffer, buf_size, 0,
                (struct sockaddr*) client_address, client_address_length);
        printf("Slept for timer: %d.%d us\n", random_delay / 1000,
                random_delay % 1000);
        return 1;
    }
    printf("Return packet dropped\n");
    return 0;
}

void tcp_loop() {
    struct sockaddr_in client_address;
    int client_address_length, client_socket;
    // We listen for connections now

    if (listen(server_socket, MAX_CONCURRENT_CLIENTS) < 0) {
        fprintf(stderr,
                "tcp_loop(): Error: Unable to listen for connections.\n");
        close(server_socket);
        exit(1);
    }

    // We accept connections indefinitely in a loop
    while (1) {

        if ((client_socket = accept(server_socket,
                (struct sockaddr*) &client_address,
                (socklen_t*) &client_address_length)) < 0) {
            fprintf(stderr,
                    "tcp_loop(): Error: Unable to accept connection.\n");
            exit(1);
        }
        while (1) {
            struct ping_packet buffer;
            int bytes_read;
            //memset(buffer, 0, MAX_BUFFER_SIZE + 1);
            if ((bytes_read = read(client_socket, &buffer, sizeof(buffer)))
                    <= 0) {
                fprintf(stderr,
                        "tcp_loop(): Error: No data read from client.\n");
                close(client_socket);
                exit(1);
            }
            display(&buffer, &client_address);
            modified_write_tcp(client_socket, &buffer,
                    sizeof(struct ping_packet));
        }
    }
}

void udp_loop() {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    // We accept connections indefinitely in a loop
    while (1) {


        memset((char*)&(client_address.sin_addr.s_addr), 0, sizeof(client_address.sin_addr.s_addr));
        struct ping_packet buffer;
        int bytes_read;
        if ((bytes_read = recvfrom(server_socket, &buffer,
                        sizeof(struct ping_packet), 0,
                        (struct sockaddr*) &client_address,
                        (socklen_t*) &client_address_length)) > 0) {
            display(&buffer, &client_address);
            modified_write_udp(&buffer,
                    sizeof(struct ping_packet), server_socket, &client_address,
                    client_address_length);
        }

    }
}

int connect_and_bind() {
    // We define a server_socket (type depends on the protocol chosen)

    int type = (protocol == TCP_PROTOCOL ? SOCK_STREAM : SOCK_DGRAM);
    if ((server_socket = socket(AF_INET, type, 0)) < 0) {
        perror(
                "connect_and_bind(): Error: Unable to create server socket. Exiting: ");
        exit(1);
    }

    // The socket is bound to an address using the bind() call
    // We define an address
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    // For the Internet domain, if we specify the special IP address INADDR_ANY (defined in
    // <netinet/in.h>), the socket endpoint will be bound to all the system’s network
    // interfaces. This means that we can receive packets from any of the network interface
    // cards installed in the system.
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror(
                "connect_and_bind(): Error: Unable to bind server socket. Exiting: ");
        exit(1);
    }
}

int main(int argc, char** argv) {
    // Creating a socket: IPv4 domain. Protocol for this is user-specified
    if (argc < 3) {
        usage(argv);
    }
    initialize_global();

    // Parses the command line options to get port number and protocol
    options(argc, argv);
    connect_and_bind();
    // TCP and UDP diverge here. For TCP, we need to listen for connections,
    // accept an incoming connection and perform read/write
    // For UDP, we simply read from the port.
    if (protocol == TCP_PROTOCOL)
        tcp_loop();
    else
        udp_loop();

    return 0;
}
