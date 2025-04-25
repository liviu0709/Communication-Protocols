#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include "comms.h"

using namespace std;

#define STDIN_BUF_SIZE 100

class Client {
    private:
        const int tcp_socket;
        struct sockaddr_in server_addr;
        char id[11];

        struct pollfd poll_fds[2];
        int poll_cnt = 0;

        void handle_stdin() {
            char buffer[STDIN_BUF_SIZE];
            scanf("%s", buffer);

            if ( strcmp(buffer, "exit") == 0 ) {
                close(tcp_socket);
                exit(0);
            }

            client_packet packet;
            char topic[STDIN_BUF_SIZE];
            scanf("%s", topic);

            if ( strcmp(buffer, "subscribe") == 0 ) {
                packet.type = CLIENT_SUBSCRIBE_TYPE;
                printf("Subscribed to topic %s\n", topic);
            } else if ( strcmp(buffer, "unsubscribe") == 0 ) {
                printf("Unsubscribed from topic %s\n", topic);
                packet.type = CLIENT_UNSUBSCRIBE_TYPE;
            } else {
                perror("Invalid command\n");
                return;
            }
            packet.len = strlen(topic) + 1;
            memcpy(packet.data, topic, packet.len);
            packet.data[packet.len] = '\0';
            int bytes_sent = send(tcp_socket, &packet, sizeof(packet), 0);
            if ( bytes_sent != sizeof(packet) ) {
                perror("TCP send failed -> Handling stdin failed miserably\n");
                exit(1);
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

            if ( strlen(name) > 10 ) {
                printf("ID too long, max length is 10\n");
                exit(1);
            }

            memcpy(id, name, strlen(name));
            id[strlen(name)] = '\0';

            printf("Registered id: %s\n", id);

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

            int bytes_sent = send(tcp_socket, &packet, sizeof(packet), 0);

            if ( bytes_sent != sizeof(packet) ) {
                printf("TCP send failed, sent %d bytes\n", bytes_sent);
                exit(1);
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
                            char buffer[1024];
                            int bytes_received = recv(tcp_socket, buffer, sizeof(buffer), 0);
                            if ( bytes_received <= 0 ) {
                                perror("TCP receive failed\n");
                                exit(1);
                            }
                            buffer[bytes_received] = '\0';
                            printf("Received: %s\n", buffer);
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
    if ( argc != 4 ) {
        printf("./client [name] [ip] [port]\n");
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    Client client(argv[1], argv[2], atoi(argv[3]));
    client.start();
}