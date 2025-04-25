using namespace std;

#define CLIENT_ID_TYPE 1

struct client_packet {
    int type;
    int len;
    char data[200];
};