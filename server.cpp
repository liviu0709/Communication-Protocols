#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <termios.h>

using namespace std;

#define MAX_INCOMING_CONNECTIONS 10
#define MAX_CONNECTIONS 100
#define MAX_UDP_MESSAGE_SIZE 1800
#define STDIN_BUF_SIZE 50

class Server {
    private:
        int port;
        const int udp_socket, tcp_socket;
        struct sockaddr_in server_addr;

        pollfd poll_fds[MAX_CONNECTIONS];
        int poll_cnt = 0;

        void talk_to_tcp_client() {
            int client_socket;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            client_socket = accept(tcp_socket, (struct sockaddr*)&client_addr, &addr_len);
            if ( client_socket < 0 ) {
                perror("TCP accept failed\n");
                return;
            }

            poll_fds[poll_cnt].fd = client_socket;
            poll_fds[poll_cnt].events = POLLIN;
            poll_cnt++;

        }

        void talk_to_udp_client() {
            char buffer[MAX_UDP_MESSAGE_SIZE];
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            int bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);

            if ( bytes_received < 0 ) {
                perror("UDP receive failed\n");
                return;
            }

            // printf("Received UDP message: %s\n", buffer);
        }

        void talk_to_myself() {
            char buffer[STDIN_BUF_SIZE];
            int bytes_received = read(fileno(stdin), buffer, sizeof(buffer));

            if ( bytes_received < 0 ) {
                perror("STDIN read failed\n");
                return;
            }

            buffer[bytes_received] = '\0';

            if ( strcmp(buffer, "exit\n") == 0 ) {
                exit(0);
            }
        }

    public:
        Server(int port) :
            udp_socket(socket(AF_INET, SOCK_DGRAM, 0)),
            tcp_socket(socket(AF_INET, SOCK_STREAM, 0))
        {
            this->port = port;

            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(port);

            int ret;
            ret = bind(udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
            if ( ret < 0 ) {
                perror("UDP bind failed\n");
                exit(1);
            }
            ret = bind(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
            if ( ret < 0 ) {
                perror("TCP bind failed\n");
                exit(1);
            }

            listen(tcp_socket, MAX_INCOMING_CONNECTIONS);

            poll_fds[0].fd = tcp_socket;
            poll_fds[0].events = POLLIN;

            poll_fds[1].fd = udp_socket;
            poll_fds[1].events = POLLIN;

            poll_fds[2].fd = fileno(stdin);
            poll_fds[2].events = POLLIN;

            poll_cnt = 3;
        }

        void start() {
            int ret;

            printf("Server started on port %d\n", port);

            while(1) {

                ret = poll(poll_fds, poll_cnt, -1);

                if ( ret < 0 ) {
                    perror("Poll failed\n");
                    exit(1);
                }

                for ( int i = 0; i < poll_cnt; i++ ) {
                    if ( poll_fds[i].revents & POLLIN ) {
                        if ( poll_fds[i].fd == tcp_socket ) {
                            talk_to_tcp_client();
                        } else if ( poll_fds[i].fd == udp_socket ) {
                            talk_to_udp_client();
                        } else if ( poll_fds[i].fd == fileno(stdin) ) {
                            talk_to_myself();
                        }

                    }
                }
            }
        }
};

int main(int argc, char* argv[]) {
    if ( argc != 2 ) {
        printf("U forgot the port...\n");
        return 1;
    }
    int port = atoi(argv[1]);
    Server server(port);
    server.start();
}