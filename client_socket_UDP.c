/*
 * server_socket.c
 *
 *  Created on: 05-Oct-2015
 *      Author: siddhanthgupta
 */
#include <sys/socket.h>         // For socket
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
//#include <netinet/in.h>
#include <netdb.h>              // For gethostbyname

void usage(char* program) {
    printf("USAGE: %s <Server IP> <Port Number>\n", program);
}

struct ping_packet {
    int seq_no;
    char message[255];
    struct timeval timestamp;
};

int timeval_subtract(struct timeval *result, struct timeval *x,
        struct timeval *y) {
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

struct ping_packet* make_packet(int seq_no) {
    struct ping_packet* packet = (struct ping_packet*) malloc(
            sizeof(struct ping_packet));
    sprintf(packet->message, "ECHO");
    gettimeofday(&(packet->timestamp), NULL);
    packet->seq_no = seq_no;
    return packet;
}

void process_reply(struct ping_packet* buffer, int bytes_read,
        struct sockaddr_in* server_address) {
    char s[INET_ADDRSTRLEN + 1];
    if (!inet_ntop(AF_INET, &(server_address->sin_addr), s,
            INET_ADDRSTRLEN + 1)) {
        fprintf(stderr, "Unable to read address\n");
    } else {
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        struct timeval diff;
        timeval_subtract(&diff, &cur_time, &(buffer->timestamp));
        printf("%d bytes from %s: seq_num=%d time=%.4lf ms\n", bytes_read, s,
                buffer->seq_no,
                (int) (diff.tv_sec) * 1000.0 + (int) (diff.tv_usec) / 1000.0);
    }
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

    printf("Host given is %s:%d\n", argv[1], port_number);
//    printf("Here");
    struct sockaddr_in address;
    address.sin_family = AF_INET;
        // For the Internet domain, if we specify the special IP address INADDR_ANY (defined in
        // <netinet/in.h>), the socket endpoint will be bound to all the system’s network
        // interfaces. This means that we can receive packets from any of the network interface
        // cards installed in the system.
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = 0;
    if (bind(client_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        fprintf(stderr, "%s: Error: Unable to bind server socket.\n", argv[0]);
        exit(1);
    }
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
//    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
//            sizeof(timeout));
    int counter = 1;
    while (counter<=10) {
        sleep(1);
        struct ping_packet* packet = make_packet(counter);
        sendto(client_socket, packet, sizeof(struct ping_packet), 0,
                (struct sockaddr*) &server_address, sizeof(struct sockaddr_in));

        struct ping_packet buffer;
        int bytes_read;

        fd_set rset;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int flag = 0;
        int sel_ret = 0;
        do {
            // initialize fd_set
            FD_ZERO(&rset);
            FD_SET(client_socket, &rset);
            sel_ret = select(client_socket + 1, &rset, NULL, NULL, &timeout);
            if (sel_ret == 0) {
//                printf("Select call timed out. No data received. \n");
//                printf("Timeout is %d and %d\n", timeout.tv_sec,
//                        timeout.tv_usec);
                printf(
                        "From localhost seq_num=%d Destination Host Unreachable\n", counter);
                // TODO: get IP address of client and print it instead of "localhost"
            } else if (sel_ret < 0) {
                printf("Fatal error. Exiting.");
                exit(1);
            } else {

                if ((bytes_read = recvfrom(client_socket, &buffer,
                        sizeof(struct ping_packet), 0,
                        NULL, NULL)) <= 0) {
                    fprintf(stderr, "%s : Error: No data read from client.\n",
                            argv[0]);
                    break;
                } else {
                    if (buffer.seq_no != counter) {
                        printf("Wrong packet received\n");
                    } else {
//                        printf("Received: %s SEQ=%d TIME=%d\n", buffer.message,
//                                buffer.seq_no, buffer.timestamp.tv_sec);
                        process_reply(&buffer, bytes_read, &server_address);
                        flag = 1;
                    }
                }
            }
//            printf("flag is %d and sel_ret is %d\n", flag, sel_ret);
        } while (!flag && sel_ret);
        counter++;
    }
    return 0;
}

