#include <gtest/gtest.h>

#include <oblo/core/string/string_view.hpp>
#include <oblo/smoke/framework.hpp>

int main(int argc, char** argv)
{
    for (int i = 0; i < argc; ++i)
    {
        if (oblo::string_view{argv[i]} == "--interactive")
        {
            oblo::smoke::g_interactiveMode = true;
            break;
        }
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}