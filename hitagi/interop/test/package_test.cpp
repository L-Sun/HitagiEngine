#include "test_macros.hpp"

import interop.gtest;

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
