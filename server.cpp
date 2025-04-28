#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cmath>
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
#include <set>
#include <netinet/tcp.h>
#include "comms.h"


using namespace std;

#define MAX_INCOMING_CONNECTIONS 10
#define MAX_CONNECTIONS 100
#define STDIN_BUF_SIZE 50
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

FILE *debug = fopen("debug.txt", "w");



class ClientInfo {
    private:
        char id[11];
        int socket;
        struct sockaddr_in addr;
        vector<string> topics;
        vector<string> wildcard_topics;
        bool connected;

    public:
        ClientInfo() {
            memset(id, 0, sizeof(id));
            socket = -1;
            memset(&addr, 0, sizeof(addr));
            connected = false;
        }

        ClientInfo(char* id, int socket, struct sockaddr_in addr) {
            memcpy(this->id, id, sizeof(this->id));
            this->socket = socket;
            this->addr = addr;
            connected = true;
        }

        void add_topic(string topic) {
            // If no + or *
            if ( topic.find('+') == string::npos && topic.find('*') == string::npos ) {
                topics.push_back(topic);
            } else {
                // printf("Adding wildcard topic: %s\n", topic.c_str());
                wildcard_topics.push_back(topic);
            }
        }

        int remove_topic(string topic) {
            int rc = -1;
            for ( int i = 0; i < topics.size(); i++ ) {
                if ( topics[i] == topic ) {
                    topics.erase(topics.begin() + i);
                    // return 0;
                    rc = 0;
                }
            }
            for ( int i = 0; i < wildcard_topics.size(); i++ ) {
                if ( wildcard_topics[i] == topic ) {
                    wildcard_topics.erase(wildcard_topics.begin() + i);
                    rc = 0;
                }
            }
            return rc;
        }

        vector<string> get_topics() {
            return topics;
        }

        vector<string> get_topics_wild() {
            return wildcard_topics;
        }

        int get_socket() {
            return socket;
        }

        void set_socket(int socket) {
            this->socket = socket;
        }

        void set_connected(bool connected) {
            this->connected = connected;
        }

        bool is_connected() {
            return connected;
        }
};

class Message {
    private:
        struct sockaddr_in addr_src;
        string topic;
        unsigned int data_type;
        string type;
        string text;
        bool recognized;
        char send_buffer[MAX_UDP_MESSAGE_SIZE];
        int send_buffer_len;

    public:
        Message() {
            memset(&addr_src, 0, sizeof(addr_src));
            topic = "";
            data_type = 0;
            recognized = false;
        }

        Message(struct sockaddr_in addr_src, char* raw_data, int raw_data_len) {
            this->addr_src = addr_src;

            // Topic field is always 50 bytes
            topic = string(raw_data, 50);
            // Trim the topic string to stop at the first '\0'
            size_t null_pos = topic.find('\0');
            if (null_pos != std::string::npos) {
                topic.resize(null_pos + 1); // Resize the string to exclude everything after '\0'
            } else {
                topic.resize(51);
                topic[50] = '\0';
            }

            fprintf(debug, "Recv from udp: Topic: %s. Len: %lu\n", topic.c_str(), topic.length());

            // Data type is 1 byte
            memset(&data_type, 0, sizeof(data_type));
            memcpy((char*)&data_type, raw_data + 50, 1);

            switch(data_type) {
                case INT: {
                    // Sign byte
                    char sign = *(raw_data + 51);
                    uint32_t num;
                    memcpy((char*)&num, raw_data + 52, sizeof(num));
                    num = ntohl(num);
                    if ( sign == 0) {
                        // Positive
                        text = to_string(num);
                    } else {
                        // Negative
                        text = to_string(-1 * (int32_t)num);
                    }
                    type = "INT";
                    recognized = true;
                    break;
                }
                case SHORT_REAL: {
                    uint16_t num;
                    memcpy((char*)&num, raw_data + 51, sizeof(num));
                    num = ntohs(num);
                    if ( num % 100 > 9 ) {
                        text = to_string(num / 100 ) + "." + to_string(num % 100);
                    } else {
                        text = to_string(num / 100 ) + ".0" + to_string(num % 100);
                    }
                    type = "SHORT_REAL";
                    recognized = true;
                    break;
                }
                case FLOAT: {
                    char sign = *(raw_data + 51);
                    uint32_t num;
                    memcpy((char*)&num, raw_data + 52, sizeof(num));
                    num = ntohl(num);
                    uint8_t decimal_places = *(raw_data + 56);
                    if ( sign )
                        text = "-";
                    else
                        text = "";
                    string int_num = to_string(num);
                    if ( int_num.length() > decimal_places ) {
                        if ( decimal_places == 0 ) {
                            text += int_num;
                        } else {
                            text += int_num.substr(0, int_num.length() - decimal_places) + "." + int_num.substr(int_num.length() - decimal_places);
                        }
                    } else {
                        text += "0." + string(decimal_places - int_num.length(), '0') + int_num;
                    }
                    type = "FLOAT";
                    recognized = true;
                    break;
                }
                case STRING: {
                    text = string(raw_data + 51, raw_data_len - 51);
                    type = "STRING";
                    recognized = true;
                    break;
                }
                default:
                fprintf(stderr, "Unknown data type: %d\n", data_type);
                recognized = false;
                return;
            }
            send_buffer_len = sprintf(send_buffer, "%s:%d - %s - %s - %s\n", inet_ntoa(addr_src.sin_addr),
            ntohs(addr_src.sin_port), topic.c_str(), type.c_str(), text.c_str());
        }

        void notify_subs(unordered_map<string, set<string>> subs, unordered_map<string, ClientInfo> &clients, vector<string> &clients_ids) {
            string topic_no_final_slash = topic;
            // Is this actually needed?
            if (!topic_no_final_slash.empty()) {
                topic_no_final_slash.erase(topic_no_final_slash.length() - 1);
            }

            // Clients ids -> bool
            unordered_map<string, bool> sent;

            memmove(send_buffer + 4, send_buffer, send_buffer_len);
            memcpy(send_buffer, &send_buffer_len, 4);
            for ( auto sub : subs[topic] ) {
                // First check if he is connected
                if ( clients[sub].is_connected() == false ) {

                    continue;
                }
                int bytes_sent = 0;
                while ( bytes_sent != send_buffer_len + sizeof(send_buffer_len) ) {
                    int rc = send(clients[sub].get_socket(), send_buffer, send_buffer_len + sizeof(send_buffer_len), 0);
                    fprintf(debug, "Sent %d / %lu bytes\n", rc, send_buffer_len + sizeof(send_buffer_len));
                    if ( rc < 0 ) {
                        perror("TCP send failed\n");
                        return;
                    }
                    if ( rc == 0 ) {
                        fprintf(stderr, "Client refuses to talk to us\n");
                        return;
                    }
                    bytes_sent += rc;
                }
                sent[sub] = true;
            }

            // Check wildcards of the clients that didnt get the message
            for ( auto id : clients_ids ) {
                // printf("Checking client %s\n", id.c_str());
                if ( sent.find(id) != sent.end() ) {
                    continue;
                }
                if ( clients[id].is_connected() == false ) {
                    // printf("Client %s is not connected\n", sub.c_str());
                    continue;
                }
                // printf("Found client(wildcard) %s\n", id.c_str());
                for ( auto topic_wildcard : clients[id].get_topics_wild() ) {
                    // Is this actually needed?
                    if (!topic_wildcard.empty()) {
                        topic_wildcard.erase(topic_wildcard.length() - 1);
                    }

                    if ( match(topic_wildcard, topic_no_final_slash) ) {
                        int bytes_sent = 0;
                        while ( bytes_sent != send_buffer_len + sizeof(send_buffer_len) ) {
                            int rc = send(clients[id].get_socket(), send_buffer, send_buffer_len + sizeof(send_buffer_len), 0);
                            fprintf(debug, "Sent %d / %lu bytes\n", rc, send_buffer_len + sizeof(send_buffer_len));
                            if ( rc < 0 ) {
                                perror("TCP send failed\n");
                                return;
                            }
                            if ( rc == 0 ) {
                                fprintf(stderr, "Client refuses to talk to us\n");
                                return;
                            }
                            bytes_sent += rc;
                        }
                        sent[id] = true;
                        break;
                    }
                }
            }
        }
};

class Server {
    private:
        int port;
        const int udp_socket, tcp_socket;
        struct sockaddr_in server_addr;

        // pollfd poll_fds[MAX_CONNECTIONS];
        vector<pollfd> poll_fds;
        int poll_cnt = 0;

        // Client FD -> bool
        unordered_map<int, bool> waiting_for_name;
        unordered_map<int, sockaddr_in> client_addrs;

        // Client FD -> Client ID
        unordered_map<int, string> clients_by_fd;
        unordered_map<string, ClientInfo> clients;
        vector<string> clients_ids;
        // Topic -> Subscribers
        unordered_map<string, set<string>> topics;

        void remove_and_close_fd(int fd) {
            close(fd);
            for ( int i = 0; i < poll_fds.size() ; i++ ) {
                if ( poll_fds[i].fd == fd ) {
                    poll_fds.erase(poll_fds.begin() + i);
                    break;
                }
            }
        }

        int recv_with_error_checks(int fd, char* buf, int len) {
            int rc = recv(fd, buf, MAX_UDP_MESSAGE_SIZE, 0);
            if ( rc < 0 ) {
                perror("TCP receive failed\n");
                exit(1);
            }
            if ( rc == 0 ) {
                return 0;
            }
            return rc;
        }

        void get_client_id(int fd) {
            char buf[MAX_UDP_MESSAGE_SIZE];
            client_packet packet;
            int bytes_received = 0;

            while ( 1 ) {
                int rc = recv_with_error_checks(fd, ((char*)&buf) + bytes_received, MAX_UDP_MESSAGE_SIZE);
                if ( rc == 0 ) {
                    remove_and_close_fd(fd);
                    waiting_for_name[fd] = false;
                    return;
                }
                bytes_received += rc;
                if ( bytes_received < sizeof(packet) ) {
                    continue;
                }
                memcpy(&packet, buf, sizeof(packet));
                if ( bytes_received == sizeof(packet) + packet.len ) {
                    break;
                }
            }
            waiting_for_name[fd] = false;

            if ( packet.type != CLIENT_ID_TYPE ) {
                fprintf(debug, "Invalid packet type: %d\n", packet.type);
                close(fd);
                return;
            }

            char id[11];
            memcpy(id, buf + sizeof(packet), packet.len);
            sockaddr_in client_addr = client_addrs[fd];
            // Client connected before
            if ( clients.find(string(id)) != clients.end() ) {
                if ( clients_by_fd.find(clients[string(id)].get_socket()) != clients_by_fd.end() ) {
                    printf("Client %s already connected.\n", id);
                    remove_and_close_fd(fd);
                    waiting_for_name[fd] = false;
                    return;
                } else {
                    clients[string(id)].set_socket(fd);
                    clients[string(id)].set_connected(true);
                }
            } else {
                ClientInfo client(id, fd, client_addr);
                clients[string(id)] = client;
            }
            clients_ids.push_back(string(id));
            clients_by_fd[fd] = string(id);
            printf("New client %s connected from %s:%d.\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }

        void disconnect_client(int fd) {
            printf("Client %s disconnected.\n", clients_by_fd[fd].c_str());
            remove_and_close_fd(fd);
            for ( int i = 0 ; i < clients_ids.size() ; i++ ) {
                if ( clients_ids[i] == clients_by_fd[fd] ) {
                    clients_ids.erase(clients_ids.begin() + i);
                    break;
                }
            }
            clients[clients_by_fd[fd]].set_connected(false);
            clients_by_fd.erase(fd);
        }

        void handle_subs(int fd) {
            if ( waiting_for_name[fd] ) {
                get_client_id(fd);
            } else {
                char recv_buf[MAX_UDP_MESSAGE_SIZE];
                memset(recv_buf, 0, sizeof(recv_buf));
                client_packet packet;
                int bytes_received = 0;
                bool working = true;
                int bytes_processed = 0;
                while ( working ) {
                    int rc = recv_with_error_checks(fd, &recv_buf[bytes_received], MAX_UDP_MESSAGE_SIZE);
                    if ( rc == 0 ) {
                        disconnect_client(fd);
                        return;
                    }
                    bytes_received += rc;
                    if ( bytes_received < sizeof(packet) + bytes_processed ) {
                        continue;
                    }

                    while ( 1 ) {
                        memcpy(&packet, recv_buf + bytes_processed, sizeof(packet));

                        if ( bytes_received < sizeof(packet) + packet.len + bytes_processed) {
                            break;
                        }

                        if ( packet.len <= 0 ) {
                            perror("Invalid packet length\n");
                            fprintf(debug, "Packet with %d len. Aborting handle\n", packet.len);
                            return;
                        }
                        if ( packet.type != CLIENT_SUBSCRIBE_TYPE && packet.type != CLIENT_UNSUBSCRIBE_TYPE ) {
                            printf("Invalid packet type: %d\n", packet.type);
                            return;
                        }
                        string topic(recv_buf + sizeof(packet) + bytes_processed, packet.len);

                        switch ( packet.type ) {
                            case CLIENT_SUBSCRIBE_TYPE: {
                                topics[topic].insert(clients_by_fd[fd]);
                                clients[clients_by_fd[fd]].add_topic(topic);
                                break;
                            }
                            case CLIENT_UNSUBSCRIBE_TYPE: {
                                int rc = clients[clients_by_fd[fd]].remove_topic(topic);
                                if ( rc < 0 ) {
                                    perror("Failed to remove topic from client\n");
                                    return;
                                }
                                auto &subs = topics[topic];
                                subs.erase(clients_by_fd[fd]);
                                if ( subs.size() == 0 ) {
                                    topics.erase(topic);
                                }
                                break;
                            }
                        }
                        bytes_processed += sizeof(packet) + packet.len;
                        fprintf(debug, "Processed: %d out of %d\n", bytes_processed, bytes_received);

                        if ( bytes_received == bytes_processed ) {
                            fprintf(debug, "Finally out of the loop\n");
                            working = false;
                        }

                        if ( bytes_received - bytes_processed < sizeof(packet) ) {
                            break;
                        }
                    }
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
            client_addrs[client_socket] = client_addr;

            // Disable Nagle's algorithm
            int enable = 1;
            if ( setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0 )
                perror("setsockopt(TCP_NODELAY) failed\n");

            waiting_for_name[client_socket] = true;

            pollfd new_poll_fd;
            new_poll_fd.fd = client_socket;
            new_poll_fd.events = POLLIN;
            poll_fds.push_back(new_poll_fd);
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

            Message msg(client_addr, buffer, bytes_received);
            msg.notify_subs(topics, clients, clients_ids);
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
            for ( int i = 0; i < poll_fds.size() ; i++ ) {
                if ( poll_fds[i].fd != tcp_socket && poll_fds[i].fd != udp_socket && poll_fds[i].fd != fileno(stdin) ) {
                    close(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    i--;
                }
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

            // Make ports reusable, in case we run this really fast two times in a row
            int enable = 1;
            if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                perror("setsockopt(SO_REUSEADDR) failed UDP\n");
            if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                perror("setsockopt(SO_REUSEADDR) failed TCP\n");

            // Disable Nagle's algorithm
            if (setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
                perror("setsockopt(TCP_NODELAY) failed\n");

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

            pollfd tcp, udp, terminal;
            tcp.fd = tcp_socket;
            tcp.events = udp.events = terminal.events = POLLIN;
            udp.fd = udp_socket;
            terminal.fd = fileno(stdin);

            poll_fds.push_back(tcp);
            poll_fds.push_back(udp);
            poll_fds.push_back(terminal);
        }

        void start() {
            int ret;

            // printf("Server started on port %d\n", port);

            while(1) {

                ret = poll(poll_fds.data(), poll_fds.size(), -1);

                if ( ret < 0 ) {
                    perror("Poll failed\n");
                    exit(1);
                }

                for ( int i = 0; i < poll_fds.size() ; i++ ) {
                    // if ( (poll_fds[i].revents & POLLHUP) || (poll_fds[i].revents & POLLERR) ) {
                    //     disconnect_client(poll_fds[i].fd);
                    //     break;
                    // }

                    if ( poll_fds[i].revents & POLLIN ) {
                        // printf("Current poll size: %d\n", poll_cnt);
                        if ( poll_fds[i].fd == tcp_socket ) {
                            // printf("TCP client connected\n");
                            talk_to_tcp_client();
                        } else if ( poll_fds[i].fd == udp_socket ) {
                            // printf("UDP client connected\n");
                            talk_to_udp_client();
                        } else if ( poll_fds[i].fd == fileno(stdin) ) {
                            // printf("STDIN client connected\n");
                            talk_to_myself();
                        } else {
                            // printf("TCP client %d has data for me\n", poll_fds[i].fd);
                            handle_subs(poll_fds[i].fd);
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
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    setvbuf(debug, NULL, _IONBF, 0);
    int port = atoi(argv[1]);
    Server server(port);
    server.start();
}