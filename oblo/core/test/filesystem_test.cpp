#include <gtest/gtest.h>

#include <oblo/core/filesystem/directory_watcher.hpp>
#include <oblo/core/filesystem/file.hpp>
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

    namespace
    {
        bool make_clear_directory(cstring_view filename)
        {
            return filesystem::remove_all(filename).has_value() && filesystem::create_directories(filename).has_value();
        }

        bool write_text_file(cstring_view filename, string_view content)
        {
            return filesystem::write_file(filename, as_bytes(std::span{content}), {}).has_value();
        }
    }

    TEST(directory_watcher, files_non_recursive)
    {
        EXPECT_TRUE(make_clear_directory("./directory_watcher_test/"));

        filesystem::directory_watcher w;

        EXPECT_TRUE(w.init({
            .path = "./directory_watcher_test",
            .isRecursive = false,
        }));

        string_builder buffer;

        {
            EXPECT_TRUE(write_text_file("./directory_watcher_test/a.foo", "A"));

            u32 createdEvents{};
            u32 modifiedEvents{};
            u32 eventsCount{};

            EXPECT_TRUE(w.process(
                [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                {
                    ++eventsCount;
                    createdEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::created};
                    modifiedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::modified};

                    buffer.clear();
                    const auto content = filesystem::load_text_file_into_memory(buffer, evt.path);
                    ASSERT_TRUE(content);
                    ASSERT_EQ(*content, string_view{"A"});
                }));

            ASSERT_EQ(createdEvents, 1);
            ASSERT_EQ(modifiedEvents, 1);
            ASSERT_EQ(eventsCount, 2);
        }

        {
            EXPECT_TRUE(write_text_file("./directory_watcher_test/a.foo", "B"));

            u32 createdEvents{};
            u32 modifiedEvents{};
            u32 eventsCount{};

            EXPECT_TRUE(w.process(
                [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                {
                    ++eventsCount;
                    createdEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::created};
                    modifiedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::modified};

                    buffer.clear();
                    const auto content = filesystem::load_text_file_into_memory(buffer, evt.path);
                    ASSERT_TRUE(content);
                    ASSERT_EQ(*content, string_view{"B"});
                }));

            ASSERT_EQ(createdEvents, 0);
            ASSERT_EQ(modifiedEvents, 1);
            ASSERT_EQ(eventsCount, 1);
        }

        {
            EXPECT_TRUE(filesystem::create_directories("./directory_watcher_test/bar"));

            u32 createdEvents{};
            u32 eventsCount{};

            EXPECT_TRUE(w.process(
                [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                {
                    ++eventsCount;
                    createdEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::created};

                    EXPECT_TRUE(filesystem::is_directory(evt.path));
                    EXPECT_EQ(filesystem::filename(evt.path), "bar");
                }));

            ASSERT_EQ(createdEvents, 1);
            ASSERT_EQ(eventsCount, 1);
        }

        {
            // Since it's not recursive, we should not get an event here
            EXPECT_TRUE(write_text_file("./directory_watcher_test/bar/b.foo", "B"));

            u32 eventsCount{};

            EXPECT_TRUE(w.process([&](const filesystem::directory_watcher_event&) OBLO_NOINLINE { ++eventsCount; }));

            ASSERT_EQ(eventsCount, 0);
        }

        {
            EXPECT_TRUE(filesystem::rename("./directory_watcher_test/bar", "./directory_watcher_test/baz"));

            u32 modifiedEvents{};
            u32 renameOld{};
            u32 renameNew{};
            u32 eventsCount{};

            EXPECT_TRUE(w.process(
                [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                {
                    ++eventsCount;
                    modifiedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::modified};
                    renameOld += u32{evt.eventKind == filesystem::directory_watcher_event_kind::renamed_old_name};
                    renameNew += u32{evt.eventKind == filesystem::directory_watcher_event_kind::renamed_new_name};

                    if (evt.eventKind == filesystem::directory_watcher_event_kind::renamed_old_name)
                    {
                        ASSERT_EQ(filesystem::filename(evt.path), "bar");
                    }

                    if (evt.eventKind == filesystem::directory_watcher_event_kind::renamed_new_name)
                    {
                        ASSERT_EQ(filesystem::filename(evt.path), "baz");
                    }

                    // At least on Windows we also get a modified event for the directory itself (but not for the
                    // parent)
                    if (evt.eventKind == filesystem::directory_watcher_event_kind::modified)
                    {
                        ASSERT_EQ(filesystem::filename(evt.path), "baz");
                    }
                }));

            ASSERT_EQ(modifiedEvents, 1);
            ASSERT_EQ(renameOld, 1);
            ASSERT_EQ(renameNew, 1);
            ASSERT_EQ(eventsCount, 3);
        }
    }
}