#include <cstddef>
#include <functional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;

#define PORT 8081
#define HOST "63.32.125.183"

#define BASIC_AUTH {"username=", "password="}
#define ADMIN_AUTH {"admin_username=", "username=", "password="}
#define MOVIE_STRING_ARGS {"title=", "description="}


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

            if ( status_code == "500" )
                return;

            size_t pos;
            pos = response.find("Content-Length:");
            if ( pos == string::npos ) {
                cout << "ERROR: Content-Length not found\n";
                exit(1);
            }
            int content_length = atoi(response.substr(pos + 16, response.find("\r\n", pos + 16) - (pos + 16)).c_str());

            pos = response.find("\r\n\r\n");

            // Check the content length
            if ( pos != string::npos ) {
                // Valid HTTP response
                // +4 bcz of \r\n\r\n
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

        json get_data() {
            return data;
        }

        string process_data_for_each(string field, function<string(json)> func) {
            string ret;
            if ( data.contains(field) ) {
                sort(data[field].begin(), data[field].end(), [](const json& a, const json& b) {
                    return a["id"].get<int>() < b["id"].get<int>();
                });
                for ( auto elem : data[field] ) {
                    ret += func(elem) + "\n";
                }
            } else {
                ret += get_message();
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
            request += "Connection: close\r\n";
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
            if ( session_id == "" ) {
                this->auth_header = false;
            } else {
                this->auth_header = true;
            }
            this->session_id = session_id;
            return *this;
        }

        HTTP_Request& add_JWT_token(string token) {
            if ( token == "" ) {
                this->auth_header_JWT = false;
            } else {
                this->auth_header_JWT = true;
            }
            this->session_id_JWT = token;
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
            if ( token == "" ) {
                this->auth_JWT = false;
            } else {
                this->auth_JWT = true;
            }
            this->session_id_JWT = token;
        }

        void add_session_id(string session_id) {
            if ( session_id == "" ) {
                this->auth_header = false;
            } else {
                this->auth_header = true;
            }
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

            // cout << "Request:\n" << request << "\n";

            int sent = 0;
            while ( sent <= request.size() ) {
                int val = ::send(sockfd, request.c_str() + sent, request.size(), 0);
                if ( val <= 0 ) {
                    break;
                }
                sent += val;
            }

            // cout << "Bytes sent: " << sent << "\n";

            char buffer[8192];
            int read = 0;
            // Receive response
            while ( true ) {
                int valread = recv(sockfd, buffer + read, 8192, 0);
                // cout << "Bytes read: " << valread << "\n";
                // cout << "Read: " << buffer << "\n";

                if ( valread <= 0 ) {
                    break;
                }
                read += valread;
                if ( read >= 8192 ) {
                    cout << "WE NEED BIGGER BUFFER\n";
                    break;
                }

                // If we found EOF, we can get the content length
                if ( strstr(buffer, "\r\n\r\n") != NULL ) {
                    // cout << "Found EOF\n";
                    if ( strstr(buffer, "Content-Length:") != NULL ) {
                        // cout << "Found Content-Length\n";
                        int content_length = atoi(strstr(buffer, "Content-Length:") + 16);
                        int header_length = strstr(buffer, "\r\n\r\n") - buffer + 4;
                        // cout << "Content length: " << content_length << "\n";
                        // cout << "Header length: " << header_length << "\n";
                        // cout << "Read: " << read << "\n";
                        if ( read >= content_length + header_length ) {
                            break;
                        }
                        // break;
                    }
                }
            }
            memset(buffer + read, 0, 8192 - read);
            // cout << "Reply:\n" << buffer << "\n";

            HTTP_Reply parser(buffer);


            close(sockfd);

            return parser;
        }

} ;

class Client {
private:
    Connection c;

    string get_input(string prompt) {
        string input;
        cout << prompt;
        getline(cin, input);
        return input;
    }

    template<typename T>
    T get_input_number(string prompt) {
        T id;
        while ( true ) {
            string input = get_input(prompt);
            istringstream iss(input);
            iss >> id;
            if ( iss.fail() ) {
                cout << "Id is supposed to be a number(input received):" << input << "\n";
                continue;
            }
            break;
        }
        return id;
    }

    vector<string> get_args(vector<string> prompts) {
        vector<string> args;
        for ( auto prompt : prompts ) {
            args.push_back(get_input(prompt));
        }
        return args;
    }

    void login_admin() {
        vector<string> args = get_args(BASIC_AUTH);
        json j;
        j["username"] = args[0];
        j["password"] = args[1];

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
        vector<string> args = get_args(BASIC_AUTH);
        json j;
        j["username"] = args[0];
        j["password"] = args[1];

        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/admin/users", "POST").send();
        cout << reply.get_message() << "\n";
    }

    void get_users() {
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/admin/users", "GET").send();
        cout << reply.process_data_for_each("users", [](json elem) {
            return "#" + to_string(elem["id"]) + " " + elem["username"].get<string>() + ":" + elem["password"].get<string>();
        }) << "\n";
    }

    void delete_user() {
        string username = get_input("username=");
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
        vector<string> args = get_args(ADMIN_AUTH);
        json j;
        j["username"] = args[1];
        j["password"] = args[2];
        j["admin_username"] = args[0];
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
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/movies", "GET").send();
        if ( reply.is_success() ) {
            cout << reply.process_data_for_each("movies", [](json elem) {
                return "#" + to_string(elem["id"]) + " " + elem["title"].get<string>();}) << "\n";
        } else {
            cout << reply.get_message() << "\n";
        }
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
        vector<string> args = get_args(MOVIE_STRING_ARGS);
        json j;
        j["title"] = args[0];
        j["year"] = get_input_number<int>("year=");
        j["description"] = args[1];
        j["rating"] = get_input_number<double>("rating=");
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/movies", "POST").send();
        cout << reply.get_message() << "\n";
    }

    void get_movie() {
        string id = to_string(get_input_number<int>("id="));
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/movies/" + id, "GET").send();
        cout << reply.get_message() << "\n";
        cout << reply.get_data().dump() << "\n";
    }

    void delete_movie() {
        string id = to_string(get_input_number<int>("id="));
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/movies/" + id, "DELETE").send();
        cout << reply.get_message() << "\n";
    }

    void update_movie() {
        vector<string> args = get_args(MOVIE_STRING_ARGS);
        json j;
        j["title"] = args[0];
        j["year"] = get_input_number<int>("year=");
        j["description"] = args[1];
        j["rating"] = get_input_number<double>("rating=");
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/movies/" + to_string(get_input_number<int>("id=")), "PUT").send();
        cout << reply.get_message() << "\n";
    }

    void get_collections() {
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/collections", "GET").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            cout << reply.process_data_for_each("collections", [](json elem) {
            return "#" + to_string(elem["id"]) + " " + elem["title"].get<string>();}) << "\n";
        }
    }

    void get_collection() {
        int id = get_input_number<int>("id=");
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/collections/" + to_string(id), "GET").send();
        cout << reply.get_message() << "\n";
        if ( reply.is_success() ) {
            cout << reply.get_data().dump() << "\n";
        }
    }

    void add_collection() {
        bool no_errors = true;
        string title = get_input("title=");
        json j;
        j["title"] = title;
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/collections", "POST").send();
        if ( !reply.is_success() ) {
            no_errors = false;
            cout << reply.get_message() << "\n";
            return;
        }
        string collection_id = to_string(reply.get_data()["id"].get<int>());
        int num_movies = stoi(get_input("num_movies="));

        for ( int i = 0; i < num_movies; i++ ) {
            json j;
            j["id"] = get_input_number<int>("movie_id[" + to_string(i) + "]=");
            HTTP_Reply reply2 = c.prepare_send(j, "/api/v1/tema/library/collections/" + collection_id + "/movies", "POST").send();
            cout << reply2.get_message() << "\n";
        }

        cout << "SUCCES: collection initialized\n";
    }

    void delete_collection() {
        string id = to_string(get_input_number<int>("id="));
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/collections/" + id, "DELETE").send();
        cout << reply.get_message() << "\n";
    }

    void add_movie_to_collection() {
        int id = get_input_number<int>("id=");
        string collection_id = to_string(get_input_number<int>("collectionId="));
        json j;
        j["id"] = id;
        HTTP_Reply reply = c.prepare_send(j, "/api/v1/tema/library/collections/" + collection_id + "/movies", "POST").send();
        cout << reply.get_message() << "\n";
    }

    void delete_movie_from_collection() {
        string id = to_string(get_input_number<int>("id="));
        string collection_id = to_string(get_input_number<int>("collectionId="));
        HTTP_Reply reply = c.prepare_send(json(), "/api/v1/tema/library/collections/" + collection_id + "/movies/" + id, "DELETE").send();
        cout << reply.get_message() << "\n";
    }

    void exit() {
        ::exit(0);
    }

    unordered_map<string, void(Client::*)()> command_map = {
        {"exit", &Client::exit},
        {"login_admin", &Client::login_admin},
        {"add_user", &Client::add_user},
        {"get_users", &Client::get_users},
        {"delete_user", &Client::delete_user},
        {"logout_admin", &Client::logout_admin},
        {"login", &Client::login},
        {"get_access", &Client::get_access},
        {"get_movies", &Client::get_movies},
        {"logout", &Client::logout},
        {"add_movie", &Client::add_movie},
        {"get_movie", &Client::get_movie},
        {"delete_movie", &Client::delete_movie},
        {"update_movie", &Client::update_movie},
        {"get_collections", &Client::get_collections},
        {"get_collection", &Client::get_collection},
        {"add_collection", &Client::add_collection},
        {"delete_collection", &Client::delete_collection},
        {"add_movie_to_collection", &Client::add_movie_to_collection},
        {"delete_movie_from_collection", &Client::delete_movie_from_collection}
    };

public:
    void run() {
        while (true) {
            string command;
            getline(cin, command);
            if ( command_map.find(command) != command_map.end() ) {
                (this->*command_map[command])();
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