#include <oblo/modules/module_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/modules/module_interface.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

namespace oblo
{
    namespace
    {
        module_manager* g_instance{nullptr};
    }

    module_manager& module_manager::get()
    {
        OBLO_ASSERT(g_instance != nullptr);
        return *g_instance;
    }

    module_manager::module_manager()
    {
        OBLO_ASSERT(g_instance == nullptr);
        g_instance = this;
    }

    module_manager::~module_manager()
    {
        shutdown();

        OBLO_ASSERT(g_instance == this);
        g_instance = nullptr;
    }

    void module_manager::shutdown()
    {
        std::vector<module_storage> modules;
        modules.reserve(m_modules.size());

        for (auto& [id, storage] : m_modules)
        {
            modules.emplace_back(std::move(storage));
        }

        m_modules.clear();

        std::sort(modules.begin(),
                  modules.end(),
                  [](const module_storage& lhs, const module_storage& rhs) { return lhs.loadOrder < rhs.loadOrder; });

        for (auto& m : modules)
        {
            m.ptr->shutdown();
        }
    }

    module_interface* module_manager::find(const type_id& id) const
    {
        const auto it = m_modules.find(id);
        if (it == m_modules.end())
        {
            return nullptr;
        }

        return it->second.ptr.get();
    }

    bool module_manager::load(const type_id& id, module_storage storage)
    {
        const auto [it, inserted] = m_modules.emplace(id, std::move(storage));

        if (!inserted)
        {
            return false;
        }

        try
        {
            if (!it->second.ptr->startup())
            {
                m_modules.erase(it);
                return false;
            }
        }
        catch (const std::exception&)
        {
            m_modules.erase(it);
            return false;
        }

        return true;
    }
}