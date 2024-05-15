#include <oblo/modules/module_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/modules/module_initializer.hpp>
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

    struct module_manager::module_storage
    {
        std::unique_ptr<module_interface> ptr;
        service_registry services;
        u32 loadOrder{};
    };

    struct module_manager::service_storage
    {
        dynamic_array<void*> implementations;
    };

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
        struct module_to_delete
        {
            type_id id;
            u32 loadOrder;
        };

        std::vector<module_to_delete> modules;
        modules.reserve(m_modules.size());

        for (auto& [id, storage] : m_modules)
        {
            modules.emplace_back(id, storage.loadOrder);
        }

        // We unload in reverse load order
        std::sort(modules.begin(),
            modules.end(),
            [](const module_to_delete& lhs, const module_to_delete& rhs) { return lhs.loadOrder > rhs.loadOrder; });

        for (auto& m : modules)
        {
            const auto it = m_modules.find(m.id);
            OBLO_ASSERT(it != m_modules.end());

            if (it == m_modules.end()) [[unlikely]]
            {
                continue;
            }

            it->second.ptr->shutdown();
            m_modules.erase(it);
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

    bool module_manager::load(const type_id& id, std::unique_ptr<module_interface> module)
    {
        const auto [it, inserted] = m_modules.emplace(id, module_storage{});

        if (!inserted)
        {
            return false;
        }

        service_registry services;

        try
        {
            if (!module->startup({
                    .services = &services,
                }))
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

        auto& moduleEntry = it->second;

        moduleEntry.ptr = std::move(module);
        moduleEntry.services = std::move(services);
        moduleEntry.loadOrder = ++m_nextLoadIndex;

        dynamic_array<service_entry> moduleServices;
        moduleEntry.services.fetch_services(moduleServices);

        for (auto& service : moduleServices)
        {
            m_services[service.type].implementations.push_back(service.pointer);
        }

        return true;
    }

    std::span<void* const> module_manager::find_services(const type_id& type) const
    {
        const auto it = m_services.find(type);

        if (it == m_services.end())
        {
            return {};
        }

        return it->second.implementations;
    }
}