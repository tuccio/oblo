#pragma once

#include <oblo/modules/module_interface.hpp>
#include <oblo/options/options_manager.hpp>

namespace oblo
{
    class options_module : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        bool finalize() override;

        options_manager& manager()
        {
            return m_manager;
        }

        const options_manager& manager() const
        {
            return m_manager;
        }

    private:
        options_manager m_manager;
    };
}