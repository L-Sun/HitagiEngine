#include "test_macros.hpp"

import std;
import interop.gtest;
import interop.gmock;
import interop.cxxopts;

TEST(InteropPackageTest, CommandLineAndMatcherImports) {
    cxxopts::Options options("interop-test");
    options.add_options()("count", "Number", cxxopts::value<int>());
    const char* argv[] = {"interop-test", "--count", "3"};
    const auto  result = options.parse(3, argv);
    EXPECT_TRUE(testing::Matches(testing::Eq(3))(result["count"].as<int>()));
}

TEST(InteropPackageTest, FatalAssertionReturnsFromCallerAndRecordsLocation) {
    testing::TestPartResultArray results;
    bool                         continued      = false;
    int                          assertion_line = 0;
    {
        testing::ScopedFakeTestPartResultReporter reporter(
            testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
        const auto check = [&] {
            assertion_line = __LINE__ + 1;
            ASSERT_TRUE(false) << "interop assertion";
            continued = true;
        };
        check();
    }
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results.GetTestPartResult(0).fatally_failed());
    EXPECT_EQ(results.GetTestPartResult(0).line_number(), assertion_line);
    EXPECT_STREQ(results.GetTestPartResult(0).file_name(), __FILE__);
    EXPECT_FALSE(continued);
}
