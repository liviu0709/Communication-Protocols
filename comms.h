#include <string>

using namespace std;

#define CLIENT_ID_TYPE 1
#define CLIENT_SUBSCRIBE_TYPE 2
#define CLIENT_UNSUBSCRIBE_TYPE 3

#define MAX_UDP_MESSAGE_SIZE 1800

struct client_packet {
    int type;
    int len;
};

bool match(string wild, string topic);