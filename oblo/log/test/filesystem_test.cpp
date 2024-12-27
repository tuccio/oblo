#include <gtest/gtest.h>

#include <oblo/core/filesystem/filesystem.hpp>

namespace oblo
{
    TEST(filesystem, extension)
    {
        cstring_view p = "./path/to/file.ext";
        EXPECT_EQ(filesystem::extension(p), ".ext");
    }

    TEST(filesystem, no_extension)
    {
        cstring_view p = "./path/to/file";
        EXPECT_EQ(filesystem::extension(p), "");
    }

    TEST(filesystem, filename)
    {
        cstring_view p = "./path/to/file.ext";
        EXPECT_EQ(filesystem::filename(p), "file.ext");
    }

    TEST(filesystem, stem)
    {
        cstring_view p = "./path/to/file.ext";
        EXPECT_EQ(filesystem::stem(p), "file");
    }
    TEST(filesystem, parent_path)
    {
        cstring_view p = "./path/to/file.ext";
        EXPECT_EQ(filesystem::parent_path(p), "./path/to");
    }
}