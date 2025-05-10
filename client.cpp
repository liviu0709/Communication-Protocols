#include <cstddef>
#include <string>
#include <cstdio>
#include <cstdlib>
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
    protected:
        string response;
        string status_code;
        int status_code_int;
        json data;

    public:
        HTTP_Reply(string response) {
            this->response = response;
            // cout << "Response:\n" << response << "\n";
            // Parse the response
            istringstream iss(response);
            string line;
            getline(iss, line);
            // Parse the status code
            istringstream iss2(line);
            string http_version;
            iss2 >> http_version >> status_code;
            status_code_int = atoi(status_code.c_str());
            // cout << "Return code: " << status_code << "\n";

            if ( status_code == "500" )
                return;

            size_t pos;
            pos = response.find("Content-Length:");
            if ( pos == string::npos ) {
                cout << "ERROR: Content-Length not found\n";
                exit(1);
            }
            int content_length = atoi(response.substr(pos + 16, response.find("\r\n", pos + 16) - (pos + 16)).c_str());
            // cout << "Content-Length(PARSED): " << content_length << "\n";
            pos = response.find("\r\n\r\n");
            // cout << "Content-Length(REAL)" << response.substr(pos + 4).length() << "\n";
            // Check the content length
            if ( pos != string::npos ) {
                // Valid HTTP response
                // +4 bcz of \r\n\r\n
                // cout << "Data:\n" << response.substr(pos + 4) << "\n";
                data = json::parse(response.substr(pos + 4));
            } else {
                cout << "ERROR: Invalid HTTP response\n";
                exit(1);
            }
        }

        bool is_success() {
            if ( status_code_int >= 200 && status_code_int < 300 ) {
                return true;
            } else {
                return false;
            }
        }

        bool is_internal_error() {
            if ( status_code_int >= 500 && status_code_int < 600 ) {
                return true;
            }
            return false;

        }

        string get_message() {
            string ret;
            if ( data.contains("message") ) {
                ret = "SUCCES: ";
                ret += data["message"];
            } else if ( data.contains("error") ) {
                ret = "ERROR: ";
                ret += data["error"];
            } else if ( is_success() ) {
                ret = "SUCCES: ";
                ret += status_code;
            } else {
                ret = "ERROR SERVER PROBLEM: ";
                ret += status_code;
            }
            return ret;
        }

        string get_status_code() {
            return status_code;
        }

        string get_session_id() {
            if ( response.find("Set-Cookie: session=") != string::npos ) {
                size_t pos = response.find("Set-Cookie: session=") + 20;
                size_t end = response.find(";", pos);
                return response.substr(pos, end - pos);
            } else {
                return "";
            }
        }

        string get_response() {
            return response;
        }

        string get_JWT_token() {
            if ( data.contains("token") ) {
                return data["token"];
            } else {
                return "";
            }
        }

        string get_data() {
            return data.dump();
        }
};

class HTTP_Reply_get_movies : HTTP_Reply {
    public:
        HTTP_Reply_get_movies(HTTP_Reply reply) : HTTP_Reply(reply.get_response()) {}

        string get_message() {
            string ret;
            if ( data.contains("movies") ) {
                ret = "SUCCES:\n";
                // On succes casually dump the list of users and their private data
                for ( auto elem : data["movies"] ) {
                    ret += "#";
                    ret += to_string(elem["id"]);
                    ret += " ";
                    ret += elem["title"];
                    // ret += ":";
                    // ret += elem["password"];
                    // ret += elem.dump();
                    ret += "\n";
                }
                // ret += data["movies"].dump();
            } else if ( data.contains("error") ) {
                ret = "ERROR: ";
                ret += data["error"];
            } else {
                ret = "ERROR SERVER PROBLEM: ";
                ret += status_code;
            }
            return ret;
        }
};

class HTTP_Reply_get_users : HTTP_Reply {
    public:
        HTTP_Reply_get_users(HTTP_Reply reply) : HTTP_Reply(reply.get_response()) {}

        string get_message() {
            string ret;
            if ( data.contains("users") ) {
                ret = "SUCCES:\n";
                // On succes casually dump the list of users and their private data
                for ( auto elem : data["users"] ) {
                    ret += "#";
                    ret += to_string(elem["id"]);
                    ret += " ";
                    ret += elem["username"];
                    ret += ":";
                    ret += elem["password"];
                    ret += "\n";

                }
                // ret += data["users"].dump();
            } else if ( data.contains("error") ) {
                ret = "ERROR: ";
                ret += data["error"];
            } else {
                ret = "ERROR SERVER PROBLEM: ";
                ret += status_code;
            }
            return ret;
        }
};

class HTTP_Request {
    private:
        string method;
        string endpoint;
        string content_type;
        string content_length;
        string session_id;
        bool auth_header;
        bool auth_header_JWT;
        string session_id_JWT;

    public:
        HTTP_Request(string method, string endpoint, string content_type, string content_length) {
            this->method = method;
            this->endpoint = endpoint;
            this->content_type = content_type;
            this->content_length = content_length;
            this->auth_header = false;
            this->auth_header_JWT = false;
        }

        string build() {
            string request = method + " " + endpoint + " HTTP/1.1\r\n"
                            "Host: " + HOST + "\r\n";
            if ( auth_header_JWT ) {
                request += "Authorization: Bearer " + session_id_JWT + "\r\n";
            }
            request += "Content-Type: " + content_type + "\r\n"
                            "Content-Length: " + content_length + "\r\n";
            if ( auth_header ) {
                request += "Vary: Cookie\r\n";
                request += "Cookie: session=" + session_id + "\r\n";
            }
            request += "\r\n";
            // cout << "Request:\n" << request << "\n";
            return request;
        }

        HTTP_Request& add_session_id(string session_id) {
            this->session_id = session_id;
            this->auth_header = true;
            return *this;
        }

        HTTP_Request& add_JWT_token(string token) {
            this->session_id_JWT = token;
            this->auth_header_JWT = true;
            return *this;
        }
};

class Connection {
    private:
        int sockfd;
        sockaddr_in serv_addr;
        string endpoint;
        string method;
        bool auth_header;
        string session_id;
        bool auth_JWT;
        string session_id_JWT;
        json j;
    public:
        Connection() {
            sockfd = -1;
            endpoint = "";
            auth_header = false;
            session_id = "";
            auth_JWT = false;
            session_id_JWT = "";
        }

        Connection& prepare_send(json j, string endpoint, string method) {
            this->j = j;
            this->endpoint = endpoint;
            this->method = method;
            return *this;
        }

        void add_JWT_token(string token) {
            this->auth_JWT = true;
            this->session_id_JWT = token;
        }

        void add_session_id(string session_id) {
            this->auth_header = true;
            this->session_id = session_id;
        }

        // Not actually needed?
        // string get_session_id() {
        //     return session_id;
        // }

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

            HTTP_Request builder(method, endpoint, "application/json", to_string(j.dump().size()));
            string request;
            if ( auth_header ) {
                builder = builder.add_session_id(session_id);
            }
            if ( auth_JWT ) {
                builder = builder.add_JWT_token(session_id_JWT);
            }
            request = builder.build();
            request += j.dump();
            int sent = 0;
            while ( sent <= request.size() ) {
                int val = ::send(sockfd, request.c_str() + sent, request.size(), 0);
                if ( val <= 0 ) {
                    break;
                }
                sent += val;
            }

            char buffer[8192];
            int read = 0;
            // Receive response
            while ( true ) {
                int valread = recv(sockfd, buffer + read, 8192, 0);
                // cout << "Bytes read: " << valread << "\n";
                if ( valread <= 0 ) {
                    break;
                }
                read += valread;
                if ( read >= 8192 ) {
                    cout << "WE NEED BIGGER BUFFER\n";
                    break;
                }
            }
            memset(buffer + read, 0, 8192 - read);
            cout << "Reply:\n" << buffer << "\n";

            HTTP_Reply parser(buffer);


            close(sockfd);

            return parser;
        }

} ;

class Client {
private:
    Connection c;
public:

    void login_admin() {
        string username, password;
        cout << "username=";
        cin >> username;
        cin.ignore();
        cout << "password=";
        cin >> password;
        cin.ignore();

        json j;
        j["username"] = username;
        j["password"] = password;

        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/admin/login", "POST").send();;
        while ( reply.is_internal_error() ) {
            cout << "Internal server shit, retrying...\n";
            reply = c.prepare_send(j, "/api/v1/tema/admin/login", "POST").send();
        }
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            // cout << "Session ID: " << reply.get_session_id() << "\n";
            c.add_session_id(reply.get_session_id());
        }
    }

    void add_user() {
        string username, password;
        cout << "username=";
        cin >> username;
        cin.ignore();
        cout << "password=";
        cin >> password;
        cin.ignore();

        json j;
        j["username"] = username;
        j["password"] = password;

        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/admin/users", "POST").send();
        cout << reply.get_message() << "\n";
    }

    void get_users() {
        HTTP_Reply_get_users reply(c.prepare_send(json(), "/api/v1/tema/admin/users", "GET").send());
        cout << reply.get_message() << "\n";
    }

    void delete_user() {
        cout << "username=";
        string username;
        cin >> username;
        cin.ignore();
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/admin/users/" + username, "DELETE").send();
        cout << reply.get_message() << "\n";
    }

    void logout_admin() {
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/admin/logout", "GET").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            c.add_session_id("");
        }
    }

    void login() {
        string admin_user, username, password;
        cout << "admin_username=";
        cin >> admin_user;
        cin.ignore();
        cout << "username=";
        cin >> username;
        cin.ignore();
        cout << "password=";
        cin >> password;
        cin.ignore();
        json j;
        j["username"] = username;
        j["password"] = password;
        j["admin_username"] = admin_user;
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/user/login", "POST").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            c.add_session_id(reply.get_session_id());
        }
    }

    void get_access() {
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/access", "GET").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            c.add_JWT_token(reply.get_JWT_token());
        }
    }

    void get_movies() {
        HTTP_Reply_get_movies reply(c.prepare_send(json(), "/api/v1/tema/library/movies", "GET").send());
        cout << reply.get_message() << "\n";
    }

    void logout() {
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/user/logout", "GET").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            c.add_session_id("");
            c.add_JWT_token("");
        }
    }

    void add_movie() {
        string id, title, desc, rating, year;
        cout << "id=";
        cin >> id;
        cin.ignore();
        cout << "title=";
        cin >> title;
        cin.ignore();
        cout << "year=";
        cin >> year;
        cin.ignore();
        cout << "description=";
        cin >> desc;
        cin.ignore();
        cout << "rating=";
        cin >> rating;
        cin.ignore();
        json j;
        j["title"] = title;
        j["year"] = year;
        j["description"] = desc;
        j["rating"] = rating;
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/movies", "POST").send();
        cout << reply.get_message() << "\n";
    }

    void get_movie() {
        string id;
        cout << "id=";
        cin >> id;
        cin.ignore();
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/movies/" + id, "GET").send();
        cout << reply.get_message() << "\n";
        cout << reply.get_data() << "\n";
    }

    void delete_movie() {
        string id;
        cout << "id=";
        cin >> id;
        cin.ignore();
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/movies/" + id, "DELETE").send();
        cout << reply.get_message() << "\n";
    }

    void update_movie() {
        string id, title, desc, rating, year;
        cout << "id=";
        cin >> id;
        cin.ignore();
        cout << "title=";
        cin >> title;
        cin.ignore();
        cout << "year=";
        cin >> year;
        cin.ignore();
        cout << "description=";
        cin >> desc;
        cin.ignore();
        cout << "rating=";
        cin >> rating;
        cin.ignore();
        json j;
        j["title"] = title;
        j["year"] = year;
        j["description"] = desc;
        j["rating"] = rating;
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/movies/" + id, "PUT").send();
        cout << reply.get_message() << "\n";
    }

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
                login_admin();
            } else if ( command == "add_user" ) {
                add_user();
            } else if ( command == "get_users" ) {
                get_users();
            } else if ( command == "delete_user" ) {
                delete_user();
            } else if ( command == "logout_admin" ) {
                logout_admin();
            } else if ( command == "login" ) {
                login();
            } else if ( command == "get_access" ) {
                get_access();
            } else if ( command == "get_movies" ) {
                get_movies();
            } else if ( command == "logout" ) {
                logout();
            } else if ( command == "add_movie" ) {
                add_movie();
            } else if ( command == "get_movie" ) {
                get_movie();
            } else if ( command == "delete_movie" ) {
                delete_movie();
            } else if ( command == "update_movie" ) {
                update_movie();
            } else {
                cout << "I dont know what u want from me\n";
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