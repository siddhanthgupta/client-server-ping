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
#include <unistd.h>             // For read/write functions
#define TCP_PROTOCOL 1
#define UDP_PROTOCOL 2

char server_name[1024];
int ntransmitted;
int nreceived;
double tmax;
double tmin;
double tsum;

int protocol;
int port;
const char* str_tcp;
const char* str_udp;

/*
 * Displays the ping statistics and gracefully terminates the program
 */
void finish() {
    printf("--- %s ping statistics ---\n", server_name);
    printf("%d packets transmitted, ", ntransmitted);
    printf("%d packets received, ", nreceived);

    if (ntransmitted)
        if (nreceived > ntransmitted)
            printf("-- somebody's printing up packets!\n");
        else
            printf("%d%% packet loss\n",
                    (int) (((ntransmitted - nreceived) * 100) / ntransmitted));
    if (nreceived)
        printf("round-trip min/avg/max = %.3lf/%.3lf/%.3lf ms\n", tmin,
                (tsum / (nreceived)), tmax);
    if (nreceived == 0)
        exit(1);
    exit(0);
}

void initialize_global() {
    str_tcp = "TCP";
    str_udp = "UDP";
    protocol = TCP_PROTOCOL;
    port = 0;
    ntransmitted = 0;
    nreceived = 0;
    tsum = 0;
    tmin = 1.0 / 0.0;
    tmax = -1;
//    atexit(finish);
    // SIGINT generated (Ctrl+c) will cause the finish() function to execute
    signal(SIGINT, finish);

    srand(1);
}

void usage(char** argv) {
    char* program = argv[0];
    printf("USAGE: %s [-t TCP/UDP] <Server IP> <Port Number>\n", program);
    exit(1);
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
    if (argc != 2)
        usage(argv);
    strcpy(server_name, *argv);
    argv++;
    port = atoi(*argv);
    printf("Address is %s. Protocol is %s. Port is %d\n", server_name,
            protocol == TCP_PROTOCOL ? str_tcp : str_udp, port);
}

void update_time_stats(struct timeval* timeout) {
    struct timeval temp, res;
    temp.tv_sec = 1;
    temp.tv_usec = 0;
    timeval_subtract(&res, &temp, timeout);
    double cur_time = res.tv_sec * 1000.00 + res.tv_usec / 1000.00;
    if (cur_time > tmax)
        tmax = cur_time;
    if (cur_time < tmin)
        tmin = cur_time;
    tsum += cur_time;
}

void receive_packet(int counter, struct sockaddr_in server_address, int client_socket) {
    struct ping_packet buffer;
    int bytes_read;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
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
            printf(
                    "From localhost seq_num=%d Destination Host Unreachable\n",
                    counter);
            // TODO: get IP address of client and print it instead of "localhost"
        } else if (sel_ret < 0) {
            printf("Fatal error. Exiting.");
            exit(1);
        } else {

            if (protocol == TCP_PROTOCOL)
                bytes_read = read(client_socket, &buffer,
                        sizeof(struct ping_packet));
            else
                bytes_read = recvfrom(client_socket, &buffer,
                        sizeof(struct ping_packet), 0,
                        NULL, NULL);

            if (bytes_read < 0) {
                perror("send_packets(): Error: No data read from client. ");
                break;
            } else {
                if (buffer.seq_no != counter) {
                    printf("Wrong packet received\n");
                } else {
                    process_reply(&buffer, bytes_read, &server_address);
                    flag = 1;
                    nreceived++;
                    update_time_stats(&timeout);
                }
            }
        }
    } while (!flag && sel_ret);
}

void connect_or_bind(int client_socket, struct sockaddr_in server_address) {
    // Connecting to server
    if (protocol == TCP_PROTOCOL) {
        if (connect(client_socket, (struct sockaddr*) &server_address,
                sizeof(server_address)) < 0) {
            perror("Error: Unable to connect to server. ");
            // TODO: More meaningful error message
            exit(1);
        }
    } else {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        // For the Internet domain, if we specify the special IP address INADDR_ANY (defined in
        // <netinet/in.h>), the socket endpoint will be bound to all the system’s network
        // interfaces. This means that we can receive packets from any of the network interface
        // cards installed in the system.
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = 0;
        if (bind(client_socket, (struct sockaddr*) &address, sizeof(address))
                < 0) {
            perror("connect_or_bind(): Error: Unable to bind server socket.\n");
            exit(1);
        }
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
    if (argc < 4) {
        usage(argv);
        exit(1);
    }
    initialize_global();

    // Parses the command line options to get port number and protocol
    options(argc, argv);

    int client_socket;
    struct hostent *server;
    struct sockaddr_in server_address;

    // Creating the socket
    int type = (protocol == TCP_PROTOCOL ? SOCK_STREAM : SOCK_DGRAM);
    if ((client_socket = socket(AF_INET, type, 0)) < 0) {
        fprintf(stderr, "%s: Error: Unable to create socket.\n", argv[0]);
        exit(1);
    }

    // Setting server address using the IP provided by the user
    if ((server = gethostbyname(server_name)) == NULL) {
        fprintf(stderr, "%s: Error: Invalid host.\n", argv[0]);
        exit(1);
    }

    server_address.sin_family = AF_INET;
    memcpy((char*) &(server_address.sin_addr.s_addr),
            (char*) server->h_addr_list[0], (size_t) server->h_length);
    server_address.sin_port = htons(port);

    connect_or_bind(client_socket, server_address);

    int counter = 1;
    while (counter <= 10) {
        sleep(1);
        struct ping_packet* packet = make_packet(counter);
        if (protocol == TCP_PROTOCOL)
            write(client_socket, packet, sizeof(struct ping_packet));
        else
            sendto(client_socket, packet, sizeof(struct ping_packet), 0,
                    (struct sockaddr*) &server_address,
                    sizeof(struct sockaddr_in));
        ntransmitted++;
        receive_packet(counter, server_address, client_socket);
        counter++;
    }
    finish();
    return 0;
}

