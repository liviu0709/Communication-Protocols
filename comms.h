using namespace std;

#define CLIENT_ID_TYPE 1
#define CLIENT_SUBSCRIBE_TYPE 2
#define CLIENT_UNSUBSCRIBE_TYPE 3

struct client_packet {
    int type;
    int len;
    char data[200];
};