module;
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

export module interop.gtest;

export namespace testing {
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::InitGoogleTest;
using ::testing::Message;
using ::testing::ScopedFakeTestPartResultReporter;
using ::testing::Test;
using ::testing::TestInfo;
using ::testing::TestParamInfo;
using ::testing::TestPartResult;
using ::testing::TestPartResultArray;
using ::testing::TestWithParam;
using ::testing::UnitTest;
using ::testing::Values;
using ::testing::ValuesIn;
}  // namespace testing
// These entities are named by the call-site macros, not a new testing API.
export namespace testing::internal {
using ::testing::internal::AlwaysFalse;
using ::testing::internal::AlwaysTrue;
using ::testing::internal::AssertHelper;
using ::testing::internal::CmpHelperFloatingPointEQ;
using ::testing::internal::CmpHelperGE;
using ::testing::internal::CmpHelperGT;
using ::testing::internal::CmpHelperLE;
using ::testing::internal::CmpHelperLT;
using ::testing::internal::CmpHelperNE;
using ::testing::internal::CmpHelperSTREQ;
using ::testing::internal::CodeLocation;
using ::testing::internal::DefaultParamName;
using ::testing::internal::DoubleNearPredFormat;
using ::testing::internal::EqHelper;
using ::testing::internal::GetBoolAssertionFailureMessage;
using ::testing::internal::GetTestTypeId;
using ::testing::internal::GetTypeId;
using ::testing::internal::GetTypeName;
using ::testing::internal::GTestNonCopyable;
using ::testing::internal::MakeAndRegisterTestInfo;
using ::testing::internal::NeverThrown;
using ::testing::internal::ParamGenerator;
using ::testing::internal::SuiteApiResolver;
using ::testing::internal::TestFactoryImpl;
using ::testing::internal::TestMetaFactory;
using ::testing::internal::TestNotEmpty;
using ::testing::internal::TrueWithString;
}  // namespace testing::internal
export {
    using ::RUN_ALL_TESTS;
}
