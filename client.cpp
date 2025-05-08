#include <string>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// HTTP Library?
#include "nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;

#define PORT 8081
#define HOST "63.32.125.183"

class HTTP_Reply {
    private:
        string status_code;

    public:
        HTTP_Reply(string response) {
            // Parse the response
            istringstream iss(response);
            string line;
            getline(iss, line);
            // Parse the status code
            istringstream iss2(line);
            string http_version;
            iss2 >> http_version >> status_code;
            // cout << "Return code: " << status_code << "\n";
        }

        bool is_success() {
            if ( status_code == "200" ) {
                return true;
            } else {
                return false;
            }
        }

        string get_status_code() {
            return status_code;
        }
};

class HTTP_Request {
    private:
        // For requests
        string method;
        string endpoint;
        string content_type;
        string content_length;
        // For responses
        string status_code;
    public:
        HTTP_Request(string method, string endpoint, string content_type, string content_length) {
            this->method = method;
            this->endpoint = endpoint;
            this->content_type = content_type;
            this->content_length = content_length;
        }

        string build() {
            string request = method + " " + endpoint + " HTTP/1.1\r\n"
                            "Host: " + HOST + "\r\n"
                            "Content-Type: " + content_type + "\r\n"
                            "Content-Length: " + content_length + "\r\n"
                            "\r\n";
            return request;
        }
};

class Connection {
    private:
        int sockfd;
        sockaddr_in serv_addr;
        string endpoint;
        char buffer[1024];
        json j;
    public:
        Connection(json j, string endpoint) {
            this->j = j;
            this->endpoint = endpoint;
        }

        HTTP_Reply send() {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if ( sockfd < 0 ) {
                cout << "Error creating socket\n";
                exit(1);
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, HOST, &serv_addr.sin_addr);

            if ( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
                cout << "Connection failed\n";
                exit(1);
            }

            HTTP_Request builder("POST", endpoint, "application/json", to_string(j.dump().size()));
            string request = builder.build();
            request += j.dump();
            ::send(sockfd, request.c_str(), request.size(), 0);
            // cout << "Data sent: " << data << "\n";

            // Receive response
            int valread = read(sockfd, buffer, 1024);
            // cout << "Reply:\n" << buffer << "\n";

            HTTP_Reply parser(buffer);


            close(sockfd);

            return parser;
        }

} ;

class Client {
private:

public:

    void run() {
        while (true) {
            string line(100, '\0');
            getline(cin, line);
            // istringstream used to parse the line
            istringstream iss(line);
            string command;
            iss >> command;
            // Single word command
            if ( command == "exit" ) {
                exit(0);
            } else if ( command == "login_admin" ) {
                string username, password;
                cout << "username=";
                cin >> username;
                cout << "password=";
                cin >> password;
                // iss >> username >> password;
                // cout << "Se incearca autentificare adminului...User: " << username << " password: " << password << "\n";

                json j;
                j["username"] = username;
                j["password"] = password;

                Connection c(j, "/api/v1/tema/admin/login");
                HTTP_Reply reply = c.send();

                if ( reply.is_success() ) {
                    cout << "SUCCESS: Admin logat cu succes!\n";
                } else {
                    cout << "ERROR: " << reply.get_status_code() << "\n";
                }

            }
        }
    };
};

int main(void) {
    auto* client = new Client();
    client->run();
    delete client;
    return 0;
}