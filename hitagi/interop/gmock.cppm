module;
#include <gmock/gmock.h>

export module interop.gmock;
import std;
import interop.gtest;

export namespace testing {
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Matches;
using ::testing::NiceMock;
using ::testing::StrictMock;
}  // namespace testing
