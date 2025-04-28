#include <gtest/gtest.h>
#include <string>
#include "comms.h"
using namespace std;

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// Test cases
TEST(MatchFunctionTest, SingleLevelWildcard) {
    EXPECT_TRUE(match("+", "test"));
    EXPECT_TRUE(match("test/+", "test/level"));
    EXPECT_FALSE(match("test/+", "test"));
    EXPECT_TRUE(match("+/test", "funnyshit/test"));
    EXPECT_FALSE(match("sike/+/ratata", "sike/ratata"));
    EXPECT_TRUE(match("sike/+/ratata", "sike/hibiratata/ratata"));
    EXPECT_TRUE(match("sike/+/ratata", "sike/ratata/ratata"));
}

TEST(MatchFunctionTest, MultiLevelWildcard) {
    EXPECT_TRUE(match("*", "test/level"));
    EXPECT_TRUE(match("test/*", "test/level"));
    EXPECT_FALSE(match("test/*", "other/level"));
    EXPECT_TRUE(match("test/*/level", "test/somrthin/dev/null/level"));
    EXPECT_TRUE(match("test/*/level", "test/somrthin/level"));
    EXPECT_TRUE(match("test/*/level", "test/level/level"));
}

TEST(MatchFunctionTest, ExactMatch) {
    EXPECT_TRUE(match("test", "test"));
    EXPECT_FALSE(match("test", "other"));
}

TEST(MatchFunctionTest, TesteleLor) {
    EXPECT_TRUE(match("+/ec/100/pressure", "upb/ec/100/pressure"));
    EXPECT_TRUE(match("*/pressure", "upb/ec/100/pressure"));
    EXPECT_TRUE(match("upb/precis/elevator/*/floor", "upb/precis/elevator/1/floor"));
    EXPECT_TRUE(match("upb/precis/100/+", "upb/precis/100/temperature"));
    EXPECT_TRUE(match("upb/+/100/temperature", "upb/precis/100/temperature"));
    EXPECT_TRUE(match("upb/+/100/+", "upb/precis/100/temperature"));
    EXPECT_TRUE(match("upb/ec/100/+", "upb/ec/100/temperature"));
    EXPECT_FALSE(match("upb/ec/100/+", "upb/ec/100/schedule/tuesday/12"));
    EXPECT_TRUE(match("*/pressure", "upb/precis/100/pressure"));
}

TEST(MatchFunctionTest, demo) {
    EXPECT_FALSE(match("pressure", "people"));
    EXPECT_FALSE(match("*/pressure", "upb/precis/elevator/1/people"));
    EXPECT_TRUE(match("+", "xyz"));
}
