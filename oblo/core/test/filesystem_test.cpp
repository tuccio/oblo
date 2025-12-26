#include <gtest/gtest.h>

#include <oblo/core/filesystem/directory_watcher.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>

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
        string_builder parentBuilder;
        cstring_view p = "./path/to/file.ext";
        EXPECT_EQ(filesystem::parent_path(p, parentBuilder), "./path/to");
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

        template <bool IsRecursive>
        void watch_directory_test()
        {
            EXPECT_TRUE(make_clear_directory("./directory_watcher_test/"));

            filesystem::directory_watcher w;

            EXPECT_TRUE(w.init({
                .path = "./directory_watcher_test",
                .isRecursive = IsRecursive,
            }));

            string_builder buffer;

            {
                EXPECT_TRUE(write_text_file("./directory_watcher_test/a.foo", "A"));

                u32 addedEvents{};
                u32 modifiedEvents{};
                u32 eventsCount{};

                EXPECT_TRUE(w.process(
                    [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                    {
                        ++eventsCount;
                        addedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::added};
                        modifiedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::modified};

                        buffer.clear();
                        const auto content = filesystem::load_text_file_into_memory(buffer, evt.path);
                        ASSERT_TRUE(content);
                        ASSERT_EQ(*content, string_view{"A"});
                    }));

                ASSERT_EQ(addedEvents, 1);
                ASSERT_GE(modifiedEvents, 1);
                ASSERT_EQ(eventsCount, addedEvents + modifiedEvents);
            }

            {
                EXPECT_TRUE(write_text_file("./directory_watcher_test/a.foo", "B"));

                u32 addedEvents{};
                u32 modifiedEvents{};
                u32 eventsCount{};

                EXPECT_TRUE(w.process(
                    [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                    {
                        ++eventsCount;
                        addedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::added};
                        modifiedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::modified};

                        buffer.clear();
                        const auto content = filesystem::load_text_file_into_memory(buffer, evt.path);
                        ASSERT_TRUE(content);
                        ASSERT_EQ(*content, string_view{"B"});
                    }));

                ASSERT_EQ(addedEvents, 0);
                ASSERT_GE(modifiedEvents, 1);
                ASSERT_EQ(eventsCount, modifiedEvents);
            }

            {
                EXPECT_TRUE(filesystem::create_directories("./directory_watcher_test/bar"));

                u32 addedEvents{};
                u32 eventsCount{};

                EXPECT_TRUE(w.process(
                    [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                    {
                        ++eventsCount;
                        addedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::added};

                        EXPECT_TRUE(filesystem::is_directory(evt.path));
                        EXPECT_EQ(filesystem::filename(evt.path), "bar");
                    }));

                ASSERT_EQ(addedEvents, 1);
                ASSERT_EQ(eventsCount, 1);
            }

            {
                EXPECT_TRUE(write_text_file("./directory_watcher_test/bar/b.foo", "B"));

                u32 addedEvents{};
                u32 eventsCount{};

                EXPECT_TRUE(w.process(
                    [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                    {
                        ++eventsCount;
                        addedEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::added};
                    }));

                if constexpr (!IsRecursive)
                {
                    // Since it's not recursive, we should not get an event here
                    ASSERT_EQ(eventsCount, 0);
                    ASSERT_EQ(addedEvents, 0);
                }
                else
                {
                    ASSERT_EQ(eventsCount, 3);
                    ASSERT_EQ(addedEvents, 1);
                }
            }

            {
                EXPECT_TRUE(filesystem::rename("./directory_watcher_test/bar", "./directory_watcher_test/baz"));

                u32 renameEvents{};

                EXPECT_TRUE(w.process(
                    [&](const filesystem::directory_watcher_event& evt) OBLO_NOINLINE
                    {
                        renameEvents += u32{evt.eventKind == filesystem::directory_watcher_event_kind::renamed};

                        if (evt.eventKind == filesystem::directory_watcher_event_kind::renamed)
                        {
                            ASSERT_EQ(filesystem::filename(evt.path), "baz");
                            ASSERT_EQ(filesystem::filename(evt.previousName), "bar");
                        }

                        // At least on Windows we also get a modified event when set as non-recursive
                        if (evt.eventKind == filesystem::directory_watcher_event_kind::modified)
                        {
                            ASSERT_EQ(filesystem::filename(evt.path), "baz");
                        }
                    }));

                ASSERT_EQ(renameEvents, 1);
            }
        }
    }

    TEST(directory_watcher, watch_directory_non_recursive)
    {
        watch_directory_test<false>();
    }

     TEST(directory_watcher, watch_directory_recursive)
    {
        watch_directory_test<true>();
    }
}