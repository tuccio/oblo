#include <gtest/gtest.h>

#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/scene/scene_module.hpp>

int main(int argc, char** argv)
{
    oblo::module_manager moduleManager;

    moduleManager.load<oblo::scene_module>();
    moduleManager.load<oblo::importers::importers_module>();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}