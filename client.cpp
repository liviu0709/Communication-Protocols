#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <netinet/tcp.h>
#include "comms.h"

using namespace std;

#define STDIN_BUF_SIZE 100

FILE *debug = fopen("debug_client.txt", "a");

class Client {
    private:
        const int tcp_socket;
        struct sockaddr_in server_addr;
        char id[11];

        struct pollfd poll_fds[2];
        int poll_cnt = 0;

        void handle_server() {
            char buffer[MAX_UDP_MESSAGE_SIZE];
            char print_buffer[MAX_UDP_MESSAGE_SIZE];
            memset(buffer, 0, sizeof(buffer));
            char *pointer_print = buffer;
            char *pointer_read = buffer;
            int bytes_received = 0;
            bool working = true;
            while ( working ) {
                int rc = recv(tcp_socket, pointer_read, sizeof(buffer) - bytes_received, 0);
                fprintf(debug, "Received %d bytes\n", rc);
                if ( rc < 0 ) {
                    perror("TCP receive failed\n");
                    return;
                }
                if ( rc == 0 ) {
                    fprintf(debug, "Server closed the connection\n");
                    close(tcp_socket);
                    exit(1);
                }
                pointer_read += rc;
                bytes_received += rc;
                int msg_len;
                msg_len = *(int*)pointer_print;
                while ( bytes_received >= sizeof(int) ) {
                    while ( bytes_received >= msg_len + sizeof(int) ) {
                        fprintf(debug, "Received message of length %d. but i have %d bytes\n", msg_len, bytes_received);
                        memcpy(print_buffer, pointer_print + sizeof(int), msg_len);
                        print_buffer[msg_len] = '\0';
                        printf("%s", print_buffer);
                        fprintf(debug, "%s", print_buffer);
                        pointer_print += msg_len + sizeof(int);
                        bytes_received -= msg_len + sizeof(int);
                        if ( bytes_received < sizeof(int) )
                            break;
                        msg_len = *(int*)pointer_print;
                    }
                    if ( bytes_received == 0 ) {
                        working = false;
                        fprintf(debug, "Stopping reading with %d bytes received and msg len %d\n", bytes_received, msg_len);
                        break;
                    }

                }
            }
        }

        void handle_stdin() {
            char buffer[STDIN_BUF_SIZE];
            scanf("%s", buffer);

            if ( strcmp(buffer, "exit") == 0 ) {
                close(tcp_socket);
                exit(0);
            }

            client_packet packet;
            if ( strcmp(buffer, "subscribe") == 0 ) {
                packet.type = CLIENT_SUBSCRIBE_TYPE;
                // printf("Subscribed to topic %s\n", topic);
            } else if ( strcmp(buffer, "unsubscribe") == 0 ) {
                // printf("Unsubscribed from topic %s\n", topic);
                packet.type = CLIENT_UNSUBSCRIBE_TYPE;
            } else {
                printf("Invalid command\n");
                return;
            }
            char topic[STDIN_BUF_SIZE];
            scanf("%s", topic);

            packet.len = strlen(topic) + 1;
            memcpy(packet.data, topic, packet.len);
            packet.data[packet.len] = '\0';
            int bytes_sent = 0;
            while ( bytes_sent != sizeof(packet) ) {
                int rc = send(tcp_socket, &packet, sizeof(packet), 0);
                if ( rc <= 0 ) {
                    perror("TCP send failed -> Handling stdin failed miserably\n");
                    fprintf(debug, "TCP send failed, sent %d bytes / %lu\n", bytes_sent, sizeof(packet));
                    exit(1);
                }
                bytes_sent += rc;
            }

            if ( strcmp(buffer, "subscribe") == 0 ) {
                printf("Subscribed to topic %s\n", topic);
                fprintf(debug, "Subscribed to topic %s\n", topic);
            } else if ( strcmp(buffer, "unsubscribe") == 0 ) {
                printf("Unsubscribed from topic %s\n", topic);
                fprintf(debug, "Unsubscribed from topic %s\n", topic);
            }
        }

    public:
        Client(const char* name, const char* ip, int port)
        : tcp_socket(socket(AF_INET, SOCK_STREAM, 0))
        {
            if ( tcp_socket < 0 ) {
                perror("TCP socket creation failed\n");
                exit(1);
            }

            // Disable Nagle's algorithm
            int enable = 1;
            if ( setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0 )
                perror("setsockopt(TCP_NODELAY) failed\n");

            if ( strlen(name) > 10 ) {
                printf("ID too long, max length is 10\n");
                exit(1);
            }

            memcpy(id, name, strlen(name));
            id[strlen(name)] = '\0';

            // printf("Registered id: %s\n", id);

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &server_addr.sin_addr);

            if ( connect(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 ) {
                perror("TCP connect failed\n");
                exit(1);
            }

            client_packet packet;
            packet.type = CLIENT_ID_TYPE;
            packet.len = strlen(id) + 1;
            memcpy(packet.data, id, packet.len);

            int bytes_sent = 0;

            while ( bytes_sent != sizeof(packet) ) {
                int rc = send(tcp_socket, &packet, sizeof(packet), 0);
                if ( rc < 0 ) {
                    fprintf(debug, "TCP send failed, sent %d bytes / %lu\n", bytes_sent, sizeof(packet));
                    exit(1);
                }
                // printf("STOP IGNORING ME. Bytes sent: %d\n", rc);
                bytes_sent += rc;
            }

            poll_fds[0].fd = tcp_socket;
            poll_fds[0].events = POLLIN;

            poll_fds[1].fd = fileno(stdin);
            poll_fds[1].events = POLLIN;

            poll_cnt = 2;
        }

        void start() {
            while (1) {
                int ret = poll(poll_fds, poll_cnt, -1);
                if ( ret < 0 ) {
                    perror("Poll failed\n");
                    exit(1);
                }

                for ( int i = 0; i < poll_cnt; i++ ) {
                    if ( poll_fds[i].revents & POLLIN ) {
                        if ( poll_fds[i].fd == tcp_socket ) {
                            handle_server();
                        } else if ( poll_fds[i].fd == fileno(stdin) ) {
                            handle_stdin();
                        }
                    }
                    if ( poll_fds[i].revents & POLLHUP ) {
                        if ( poll_fds[i].fd == tcp_socket ) {
                            // printf("Server closed the connection\n");
                            // close(tcp_socket);
                            exit(1);
                        }
                    }


                }
            }
        }
};

int main(int argc, char* argv[]) {
    fprintf(debug, "New client started\n");
    if ( argc != 4 ) {
        printf("./client [name] [ip] [port]\n");
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    setvbuf(debug, NULL, _IONBF, BUFSIZ);
    Client client(argv[1], argv[2], atoi(argv[3]));
    client.start();
}