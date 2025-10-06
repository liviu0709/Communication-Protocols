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
            if ( topic_coord == topic.length() && i + 1 == wild.length() )
                return true;
            if ( topic_coord == topic.length() ) {
                return false;
            }
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
                while(1) {
                while ( topic_coord < topic.length() && wild[i + 1] != topic[topic_coord] ) {
                    topic_coord++;
                }
                topic_coord++;
                if ( topic_coord >= topic.length() || i + 2 >= wild.length() ) {
                    return false;
                }
                if ( match(wild.substr(i + 2), topic.substr(topic_coord)) )
                    return true;
                if ( topic_coord == topic.length() - 1)
                    break;
                }

            } else if ( i == wild.length() - 1) {
                // * is last.. so its gud
                return true;
            }
        } else if ( wild[i] != topic[topic_coord] ) {
            return false;
        } else {
            topic_coord++;
        }
    }
    return true;
}