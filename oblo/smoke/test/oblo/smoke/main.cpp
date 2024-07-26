#include <gtest/gtest.h>

#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/smoke/framework.hpp>

int main(int argc, char** argv)
{
    oblo::module_manager moduleManager;

    moduleManager.load<oblo::scene_module>();
    moduleManager.load<oblo::importers::importers_module>();

    for (int i = 0; i < argc; ++i)
    {
        if (string_view{argv[i]} == "--interactive")
        {
            oblo::smoke::g_interactiveMode = true;
            break;
        }
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}