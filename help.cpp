#include <string>

using namespace std;

bool match(string wild, string topic) {
    int wild_coord = 0;
    int topic_coord = 0;

    for ( int i = 0; i < wild.length(); i++ ) {

        if ( wild[i] == '+') {
            // Logic for 1 level
            while ( topic[topic_coord] != '/' && topic_coord < topic.length() ) {
                topic_coord++;
            }
            // printf("Stopped at wild [%c] and normal [%c]\n", wild[i], topic[topic_coord]);
            if ( topic_coord == topic.length() && i + 1 == wild.length() )
                return true;
            if ( topic_coord == topic.length() ) {
                // fprintf(debug, "Am matchuit + ul cu tot topicu.. nu cred ca e ok\n");
                // fprintf(debug, "Wild : %s. Normal: %s", wild.c_str(), topic.c_str());
                return false;
            }
            // topic_coord++;
            if ( topic_coord != topic.length() && topic[topic_coord] == '/' && i + 1 == wild.length() ) {
                return false;
            }

        } else if ( wild[i] == '*' ) {
            // Logic for multiple levels
            if ( wild.length() == 1 ) {
                return true;
            }
            // * is first so we need to know the second one..
            if ( i != wild.length() - 1) {
                // first potential match
                while(1) {
                while ( topic_coord < topic.length() && wild[i + 1] != topic[topic_coord] ) {
                    // printf("Lf match between wild [%c] and normal [%c[%d]]\n", wild[i + 1], topic[topic_coord], topic_coord);
                    topic_coord++;
                }
                // i++;
                topic_coord++;
                if ( topic_coord >= topic.length() || i + 2 >= wild.length() ) {
                    return false;
                }
                // printf("Gonna match %s and %s with lens: %lu and %lu. I: %d. Topic coord: %d\n", wild.c_str(), topic.c_str(), wild.substr(i + 2).length(), topic.substr(topic_coord).length(), i, topic_coord);
                // printf("Verifications completed all gud\n");
                if ( match(wild.substr(i + 2), topic.substr(topic_coord)) )
                    return true;
                // printf("Topic coords vs length - 1: %d vs %lu\n", topic_coord, topic.length() - 1);
                if ( topic_coord == topic.length() - 1)
                    break;
                }

            } else if ( i == wild.length() - 1) {
            // * is last.. so its gud

                return true;
            }
            // else {
            //     while(1) {
            //     // * is in the middle
            //     // We treat this exactly like the first one
            //     while ( wild[i + 1] != topic[topic_coord] && topic_coord < topic.length() ) {
            //         topic_coord++;
            //     }
            //     // i++;
            //     // printf("Iter wild: %d. Iter topic: %d. Wild: %s. Topic: %s\n", i, topic_coord, wild.c_str(), topic.c_str());
            //     // printf("Wild: %c. Base: %c", wild[i], topic[topic_coord]);
            //     // printf("Wild sub: %s. Topic sub: %s", wild.substr(i).c_str(), topic.substr(topic_coord).c_str());
            //     // printf("Substrings: %s | %s | %s\n", wild.substr(i + 1).c_str(), wild.substr(i + 2).c_str(), wild.substr(i ).c_str());
            //     if ( match(wild.substr(i + 2), topic.substr(topic_coord)) )
            //         return true;
            //     topic_coord++;
            //     if ( topic_coord == topic.length() - 1)
            //         break;
            //     }
            // }

        } else if ( wild[i] != topic[topic_coord] ) {
            return false;
        } else {
            topic_coord++;
        }
    }
    return true;
}