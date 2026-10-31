#include "file_io.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{
class FileIoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_logger::messages().clear();
        dir_ = std::filesystem::temp_directory_path() / ("engine_file_io_tests_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path dir_;
};

TEST_F(FileIoTest, WritesAndReadsBack)
{
    const auto path = dir_ / "scene.json";
    ASSERT_TRUE(engine::write_file_atomic(path, "{ \"a\": 1 }", "Test"));
    const auto content = engine::read_text_file(path, "Test");
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, "{ \"a\": 1 }");
}

TEST_F(FileIoTest, ReplacesTheOldFileAndLeavesNoTemporaryFile)
{
    const auto path = dir_ / "scene.json";
    ASSERT_TRUE(engine::write_file_atomic(path, "old content which is longer", "Test"));
    ASSERT_TRUE(engine::write_file_atomic(path, "new", "Test"));
    EXPECT_EQ(engine::read_text_file(path, "Test").value_or(""), "new");
    std::size_t files = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(dir_))
    {
        files++;
    }
    EXPECT_EQ(files, 1u);
}

TEST_F(FileIoTest, AFileWhichCantBeWrittenLeavesTheOldOne)
{
    // a folder in the way of the temporary file: it can't be created
    const auto path = dir_ / "scene.json";
    ASSERT_TRUE(engine::write_file_atomic(path, "old", "Test"));
    std::filesystem::create_directories(dir_ / "scene.json.tmp");
    EXPECT_FALSE(engine::write_file_atomic(path, "new", "Test"));
    EXPECT_TRUE(test_logger::logged("Test: can't write"));
    EXPECT_EQ(engine::read_text_file(path, "Test").value_or(""), "old");
}

TEST_F(FileIoTest, AMissingFileIsLogged)
{
    EXPECT_FALSE(engine::read_text_file(dir_ / "missing.json", "Test").has_value());
    EXPECT_TRUE(test_logger::logged("Test: can't read"));
}
}  // namespace
