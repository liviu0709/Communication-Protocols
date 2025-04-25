#include <cstdlib>
#include <ostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <ostream>
#include <unordered_map>
#include <vector>
#include "comms.h"


using namespace std;

#define MAX_INCOMING_CONNECTIONS 10
#define MAX_CONNECTIONS 100
#define MAX_UDP_MESSAGE_SIZE 1800
#define STDIN_BUF_SIZE 50

class Message {
    private:
        struct sockaddr_in addr_src;
        string topic;
        int data_type;
        char payload[1500];
        int payload_len;

    public:
        Message() {
            memset(&addr_src, 0, sizeof(addr_src));
            topic = "";
            data_type = 0;
            memset(payload, 0, sizeof(payload));
            payload_len = 0;
        }

        Message(struct sockaddr_in addr_src, string topic, int data_type, char* payload, int payload_len) {
            this->addr_src = addr_src;
            this->topic = topic;
            this->data_type = data_type;
            memcpy(this->payload, payload, payload_len);
            this->payload_len = payload_len;
        }
};

class ClientInfo {
    private:
        char id[11];
        int socket;
        struct sockaddr_in addr;
        vector<string> topics;

    public:
        ClientInfo() {
            memset(id, 0, sizeof(id));
            socket = -1;
            memset(&addr, 0, sizeof(addr));
        }

        ClientInfo(char* id, int socket, struct sockaddr_in addr) {
            memcpy(this->id, id, sizeof(this->id));
            this->socket = socket;
            this->addr = addr;
        }

        void add_topic(string topic) {
            topics.push_back(topic);
        }

        int remove_topic(string topic) {
            for ( int i = 0; i < topics.size(); i++ ) {
                if ( topics[i] == topic ) {
                    topics.erase(topics.begin() + i);
                    return 0;
                }
            }
            return -1;
        }
};

class Server {
    private:
        int port;
        const int udp_socket, tcp_socket;
        struct sockaddr_in server_addr;

        pollfd poll_fds[MAX_CONNECTIONS];
        int poll_cnt = 0;

        // Client FD -> Client ID
        unordered_map<int, string> clients_by_fd;
        unordered_map<string, ClientInfo> clients;
        // Topic -> Subscribers
        unordered_map<string, vector<string>> topics;

        void handle_subs(int fd) {
            printf("GOING TO HANDLE SHIT\n");
            // Infinity loop when client disconnects
            client_packet packet;
            int bytes_received = recv(fd, &packet, sizeof(packet), 0);
            if ( bytes_received < 0 ) {
                perror("TCP receive failed\n");
                return;
            }
            if ( packet.len <= 0 ) {
                perror("Invalid packet length\n");
                return;
            }
            if ( packet.type != CLIENT_SUBSCRIBE_TYPE && packet.type != CLIENT_UNSUBSCRIBE_TYPE ) {
                printf("Invalid packet type: %d\n", packet.type);
                return;
            }
            string topic(packet.data, packet.len);
            switch ( packet.type ) {
                case CLIENT_SUBSCRIBE_TYPE: {
                    topics[topic].push_back(clients_by_fd[fd]);
                    clients[clients_by_fd[fd]].add_topic(topic);

                    printf("Client %s subscribed to topic %s\n", clients_by_fd[fd].c_str(), topic.c_str());
                    printf("Topic %s has %ld subscribers\n", topic.c_str(), topics[topic].size());
                    break;
                }
                case CLIENT_UNSUBSCRIBE_TYPE: {
                    int rc = clients[clients_by_fd[fd]].remove_topic(topic);
                    if ( rc < 0 ) {
                        perror("Failed to remove topic from client\n");
                        return;
                    }
                    auto &subs = topics[topic];
                    for ( int i = 0; i < subs.size(); i++ ) {
                        if ( subs[i] == clients_by_fd[fd] ) {
                            subs.erase(subs.begin() + i);
                            break;
                        }
                    }
                    if ( subs.size() == 0 ) {
                        topics.erase(topic);
                    }

                    printf("Client %s unsubscribed from topic %s\n", clients_by_fd[fd].c_str(), topic.c_str());
                    printf("Topic %s has %ld subscribers\n", topic.c_str(), topics[topic].size());
                    break;
                }
            }

        }

        void talk_to_tcp_client() {
            int client_socket;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            client_socket = accept(tcp_socket, (struct sockaddr*)&client_addr, &addr_len);
            if ( client_socket < 0 ) {
                perror("TCP accept failed\n");
                return;
            }

            client_packet packet;
            recv(client_socket, &packet, sizeof(packet), 0);
            if ( packet.type != CLIENT_ID_TYPE ) {
                printf("Invalid packet type: %d\n", packet.type);
                close(client_socket);
                return;
            }

            char id[11];
            memcpy(id, packet.data, packet.len);

            if ( clients.find(string(id)) != clients.end() ) {
                printf("Client %s already connected.\n", id);
                close(client_socket);
                return;
            }

            ClientInfo client(id, client_socket, client_addr);
            clients[string(id)] = client;
            clients_by_fd[client_socket] = string(id);

            printf("New client %s connected from %s:%d.\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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
                // Also close all clients!
                leave_me_alone();
                exit(0);
            }
        }

        void leave_me_alone() {
            for ( int i = 0; i < poll_cnt; i++ ) {
                if ( poll_fds[i].fd != tcp_socket && poll_fds[i].fd != udp_socket ) {
                    close(poll_fds[i].fd);
                }
            }
            poll_cnt = 2;
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

            // Make ports reusable, in case we run this really fast two times in a row
            int enable = 1;
            if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                perror("setsockopt(SO_REUSEADDR) failed UDP\n");
            if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                perror("setsockopt(SO_REUSEADDR) failed TCP\n");

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

            // printf("Server started on port %d\n", port);

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
                        } else {
                            handle_subs(poll_fds[i].fd);
                        }

                    }
                    if ( (poll_fds[i].revents & POLLHUP) || (poll_fds[i].revents & POLLERR) ) {
                        close(poll_fds[i].fd);
                        clients.erase(clients_by_fd[poll_fds[i].fd]);
                        clients_by_fd.erase(poll_fds[i].fd);
                        printf("Client %s disconnected.\n", clients_by_fd[poll_fds[i].fd].c_str());
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
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int port = atoi(argv[1]);
    Server server(port);
    server.start();
}