#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi;

class AppTest : public ::testing::Test {
protected:
    AppTest()
        : app(Application::CreateApp({
              .title = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
          })) {}
    std::unique_ptr<Application> app;
};

TEST_F(AppTest, CreateApp) {
    EXPECT_NE(app, nullptr);
}

TEST_F(AppTest, ResizeWindow) {
    app->ResizeWindow(800, 600);
    auto rect = app->GetWindowsRect();
    EXPECT_EQ(rect.right - rect.left, 800);
    EXPECT_EQ(rect.bottom - rect.top, 600);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    try {
        return RUN_ALL_TESTS();
    } catch (const std::bad_alloc& err) {
        std::cout << err.what() << std::endl;
    }
}