// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

// Call-site macros adapted from GoogleTest 1.17.0; the package version is not pinned.
// Types and functions are provided by import interop.gtest.
// Keep source locations, assertion return behavior and test registration here.
// This project enables exceptions and RTTI; adjust this adapter if upstream changes break compatibility.
#if !defined(__cpp_exceptions) || !defined(__cpp_rtti)
#error "The interop.gtest adapter requires exceptions and RTTI"
#endif

#define ASSERT_EQ(val1, val2) GTEST_ASSERT_EQ(val1, val2)
#define ASSERT_FALSE(condition) GTEST_ASSERT_FALSE(condition)
#define ASSERT_NE(val1, val2) GTEST_ASSERT_NE(val1, val2)
#define ASSERT_PRED_FORMAT2(pred_format, v1, v2) GTEST_PRED_FORMAT2_(pred_format, v1, v2, GTEST_FATAL_FAILURE_)
#define ASSERT_TRUE(condition) GTEST_ASSERT_TRUE(condition)
#define EXPECT_DOUBLE_EQ(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<double>, val1, val2)
#define EXPECT_EQ(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::EqHelper::Compare, val1, val2)
#define EXPECT_FALSE(condition) GTEST_EXPECT_FALSE(condition)
#define EXPECT_FLOAT_EQ(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<float>, val1, val2)
#define EXPECT_GE(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGE, val1, val2)
#define EXPECT_GT(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGT, val1, val2)
#define EXPECT_LE(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLE, val1, val2)
#define EXPECT_LT(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLT, val1, val2)
#define EXPECT_NE(val1, val2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperNE, val1, val2)
#define EXPECT_NEAR(val1, val2, abs_error) EXPECT_PRED_FORMAT3(::testing::internal::DoubleNearPredFormat, val1, val2, abs_error)
#define EXPECT_NO_THROW(statement) GTEST_TEST_NO_THROW_(statement, GTEST_NONFATAL_FAILURE_)
#define EXPECT_PRED_FORMAT2(pred_format, v1, v2) GTEST_PRED_FORMAT2_(pred_format, v1, v2, GTEST_NONFATAL_FAILURE_)
#define EXPECT_PRED_FORMAT3(pred_format, v1, v2, v3) GTEST_PRED_FORMAT3_(pred_format, v1, v2, v3, GTEST_NONFATAL_FAILURE_)
#define EXPECT_STREQ(s1, s2) EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSTREQ, s1, s2)
#define EXPECT_THROW(statement, expected_exception) GTEST_TEST_THROW_(statement, expected_exception, GTEST_NONFATAL_FAILURE_)
#define EXPECT_TRUE(condition) GTEST_EXPECT_TRUE(condition)
#define FAIL() GTEST_FAIL()
#define GTEST_AMBIGUOUS_ELSE_BLOCKER_ \
    switch (0)                        \
    case 0:                           \
    default:
#define GTEST_ASSERT_(expression, on_failure)                                                    \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_ if (const ::testing::AssertionResult gtest_ar = (expression)); \
    else on_failure(gtest_ar.failure_message())
#define GTEST_ASSERT_EQ(val1, val2) ASSERT_PRED_FORMAT2(::testing::internal::EqHelper::Compare, val1, val2)
#define GTEST_ASSERT_FALSE(condition) GTEST_TEST_BOOLEAN_(!(condition), #condition, true, false, GTEST_FATAL_FAILURE_)
#define GTEST_ASSERT_NE(val1, val2) ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperNE, val1, val2)
#define GTEST_ASSERT_TRUE(condition) GTEST_TEST_BOOLEAN_(condition, #condition, false, true, GTEST_FATAL_FAILURE_)
#define GTEST_CONCAT_TOKEN_(foo, bar) GTEST_CONCAT_TOKEN_IMPL_(foo, bar)
#define GTEST_CONCAT_TOKEN_IMPL_(foo, bar) foo##bar
#define GTEST_EXCEPTION_TYPE_(e) ::testing::internal::GetTypeName(typeid(e))
#define GTEST_EXPAND_(arg) arg
#define GTEST_EXPECT_FALSE(condition) GTEST_TEST_BOOLEAN_(!(condition), #condition, true, false, GTEST_NONFATAL_FAILURE_)
#define GTEST_EXPECT_TRUE(condition) GTEST_TEST_BOOLEAN_(condition, #condition, false, true, GTEST_NONFATAL_FAILURE_)
#define GTEST_FAIL() GTEST_FATAL_FAILURE_("Failed")
#define GTEST_FATAL_FAILURE_(message) return GTEST_MESSAGE_(message, ::testing::TestPartResult::kFatalFailure)
#define GTEST_GET_FIRST_(first, ...) first
#define GTEST_GET_SECOND_(first, second, ...) second
#define GTEST_MESSAGE_(message, result_type) GTEST_MESSAGE_AT_(__FILE__, __LINE__, message, result_type)
#define GTEST_MESSAGE_AT_(file, line, message, result_type) ::testing::internal::AssertHelper(result_type, file, line, message) = ::testing::Message()
#define GTEST_NONFATAL_FAILURE_(message) GTEST_MESSAGE_(message, ::testing::TestPartResult::kNonFatalFailure)
#define GTEST_PRED_FORMAT2_(pred_format, v1, v2, on_failure) GTEST_ASSERT_(pred_format(#v1, #v2, v1, v2), on_failure)
#define GTEST_PRED_FORMAT3_(pred_format, v1, v2, v3, on_failure) GTEST_ASSERT_(pred_format(#v1, #v2, #v3, v1, v2, v3), on_failure)
#define GTEST_SKIP() GTEST_SKIP_("")
#define GTEST_SKIP_(message) return GTEST_MESSAGE_(message, ::testing::TestPartResult::kSkip)
#define GTEST_STRINGIFY_(...) GTEST_STRINGIFY_HELPER_(__VA_ARGS__, )
#define GTEST_STRINGIFY_HELPER_(name, ...) #name
#define GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement) \
    if (::testing::internal::AlwaysTrue()) {                      \
        statement;                                                \
    } else                                                        \
        static_assert(true, "")
#define GTEST_TEST(test_suite_name, test_name) GTEST_TEST_(test_suite_name, test_name, ::testing::Test, ::testing::internal::GetTestTypeId())
#define GTEST_TEST_(test_suite_name, test_name, parent_class, parent_id)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    static_assert(sizeof(GTEST_STRINGIFY_(test_suite_name)) > 1, "test_suite_name must not be empty");                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    static_assert(sizeof(GTEST_STRINGIFY_(test_name)) > 1, "test_name must not be empty");                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    public:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)()                                                                           = default;                                                                                                                                                                                                                                                                                                                                                                                                              \
        ~GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() override                                                                 = default;                                                                                                                                                                                                                                                                                                                                                                                                              \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)(const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &)                 = delete;                                                                                                                                                                                                                                                                                                                                                                                                               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) & operator=(const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &)     = delete;                                                                                                                                                                                                                                                                                                                                                                                                               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) noexcept             = delete;                                                                                                                                                                                                                                                                                                                                                                                                               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) & operator=(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) noexcept = delete;                                                                                                                                                                                                                                                                                                                                                                                                               \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    private:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
        void                                               TestBody() override;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        [[maybe_unused]] static ::testing::TestInfo* const test_info_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    };                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    ::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ = ::testing::internal::MakeAndRegisterTestInfo(#test_suite_name, #test_name, nullptr, nullptr, ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id), ::testing::internal::SuiteApiResolver<parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__), ::testing::internal::SuiteApiResolver<parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__), new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)>); \
    void                       GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()
#define GTEST_TEST_BOOLEAN_(expression, text, actual, expected, fail)                                                       \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_ if (const ::testing::AssertionResult gtest_ar_ = ::testing::AssertionResult(expression)); \
    else fail(::testing::internal::GetBoolAssertionFailureMessage(gtest_ar_, text, #actual, #expected).c_str())
#define GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) test_suite_name##_##test_name##_Test
#define GTEST_TEST_F(test_fixture, test_name) GTEST_TEST_(test_fixture, test_name, test_fixture, ::testing::internal::GetTypeId<test_fixture>())
#define GTEST_TEST_NO_THROW_(statement, fail)                                                                                     \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_ if (::testing::internal::TrueWithString gtest_msg{}) {                                          \
        try {                                                                                                                     \
            GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);                                                            \
        }                                                                                                                         \
        GTEST_TEST_NO_THROW_CATCH_STD_EXCEPTION_()                                                                                \
        catch (...) {                                                                                                             \
            gtest_msg.value = "it throws.";                                                                                       \
            goto GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__);                                                         \
        }                                                                                                                         \
    }                                                                                                                             \
    else GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__) : fail(("Expected: " #statement " doesn't throw an exception.\n" \
                                                                         "  Actual: " +                                           \
                                                                         gtest_msg.value)                                         \
                                                                            .c_str())
#define GTEST_TEST_NO_THROW_CATCH_STD_EXCEPTION_()                    \
    catch (std::exception const& e) {                                 \
        gtest_msg.value = "it throws ";                               \
        gtest_msg.value += GTEST_EXCEPTION_TYPE_(e);                  \
        gtest_msg.value += " with description \"";                    \
        gtest_msg.value += e.what();                                  \
        gtest_msg.value += "\".";                                     \
        goto GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__); \
    }
#define GTEST_TEST_THROW_(statement, expected_exception, fail)                                                                                        \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_ if (::testing::internal::TrueWithString gtest_msg{}) {                                                              \
        bool gtest_caught_expected = false;                                                                                                           \
        try {                                                                                                                                         \
            GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);                                                                                \
        } catch (expected_exception const&) {                                                                                                         \
            gtest_caught_expected = true;                                                                                                             \
        }                                                                                                                                             \
        GTEST_TEST_THROW_CATCH_STD_EXCEPTION_(statement, expected_exception)                                                                          \
        catch (...) {                                                                                                                                 \
            gtest_msg.value = "Expected: " #statement " throws an exception of type " #expected_exception ".\n  Actual: it throws a different type."; \
            goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);                                                                               \
        }                                                                                                                                             \
        if (!gtest_caught_expected) {                                                                                                                 \
            gtest_msg.value = "Expected: " #statement " throws an exception of type " #expected_exception ".\n  Actual: it throws nothing.";          \
            goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);                                                                               \
        }                                                                                                                                             \
    }                                                                                                                                                 \
    else GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__) : fail(gtest_msg.value.c_str())
#define GTEST_TEST_THROW_CATCH_STD_EXCEPTION_(statement, expected_exception)                                                                                                                                                                  \
    catch (typename std::conditional<std::is_same<typename std::remove_cv<typename std::remove_reference<expected_exception>::type>::type, std::exception>::value, const ::testing::internal::NeverThrown&, const std::exception&>::type e) { \
        gtest_msg.value = "Expected: " #statement " throws an exception of type " #expected_exception ".\n  Actual: it throws ";                                                                                                              \
        gtest_msg.value += GTEST_EXCEPTION_TYPE_(e);                                                                                                                                                                                          \
        gtest_msg.value += " with description \"";                                                                                                                                                                                            \
        gtest_msg.value += e.what();                                                                                                                                                                                                          \
        gtest_msg.value += "\".";                                                                                                                                                                                                             \
        goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);                                                                                                                                                                           \
    }
#define INSTANTIATE_TEST_SUITE_P(prefix, test_suite_name, ...)                                                                                                                                       \
    static ::testing::internal::ParamGenerator<test_suite_name::ParamType> gtest_##prefix##test_suite_name##_EvalGenerator_() { return GTEST_EXPAND_(GTEST_GET_FIRST_(__VA_ARGS__, DUMMY_PARAM_)); } \
    static ::std::string                                                   gtest_##prefix##test_suite_name##_EvalGenerateName_(const ::testing::TestParamInfo<test_suite_name::ParamType>& info) {   \
        if (::testing::internal::AlwaysFalse()) {                                                                                                                                                    \
            ::testing::internal::TestNotEmpty(GTEST_EXPAND_(GTEST_GET_SECOND_(__VA_ARGS__, ::testing::internal::DefaultParamName<test_suite_name::ParamType>, DUMMY_PARAM_)));                       \
            auto t = std::make_tuple(__VA_ARGS__);                                                                                                                                                   \
            static_assert(std::tuple_size<decltype(t)>::value <= 2, "Too Many Args!");                                                                                                               \
        }                                                                                                                                                                                            \
        return ((GTEST_EXPAND_(GTEST_GET_SECOND_(__VA_ARGS__, ::testing::internal::DefaultParamName<test_suite_name::ParamType>, DUMMY_PARAM_))))(info);                                             \
    }                                                                                                                                                                                                \
    [[maybe_unused]] static int gtest_##prefix##test_suite_name##_dummy_ = ::testing::UnitTest::GetInstance()->parameterized_test_registry().GetTestSuitePatternHolder<test_suite_name>(GTEST_STRINGIFY_(test_suite_name), ::testing::internal::CodeLocation(__FILE__, __LINE__))->AddTestSuiteInstantiation(GTEST_STRINGIFY_(prefix), &gtest_##prefix##test_suite_name##_EvalGenerator_, &gtest_##prefix##test_suite_name##_EvalGenerateName_, __FILE__, __LINE__)
#define TEST(test_suite_name, test_name) GTEST_TEST(test_suite_name, test_name)
#define TEST_F(test_fixture, test_name) GTEST_TEST_F(test_fixture, test_name)
#define TEST_P(test_suite_name, test_name)                                                                                                                                                                                                                                                                                                                                                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name, private ::testing::internal::GTestNonCopyable {                                                                                                                                                                                                                                                                                                                     \
    public:                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                                                                                                                                                                                                                                                                                                                                                                                            \
        void TestBody() override;                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    private:                                                                                                                                                                                                                                                                                                                                                                                                                                               \
        static int AddToRegistry() {                                                                                                                                                                                                                                                                                                                                                                                                                       \
            ::testing::UnitTest::GetInstance()->parameterized_test_registry().GetTestSuitePatternHolder<test_suite_name>(GTEST_STRINGIFY_(test_suite_name), ::testing::internal::CodeLocation(__FILE__, __LINE__))->AddTestPattern(GTEST_STRINGIFY_(test_suite_name), GTEST_STRINGIFY_(test_name), new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)>(), ::testing::internal::CodeLocation(__FILE__, __LINE__)); \
            return 0;                                                                                                                                                                                                                                                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
        [[maybe_unused]] static int gtest_registering_dummy_;                                                                                                                                                                                                                                                                                                                                                                                              \
    };                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    int  GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::gtest_registering_dummy_ = GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();                                                                                                                                                                                                                                                                                               \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()
