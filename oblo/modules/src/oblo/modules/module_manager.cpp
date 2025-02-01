#include <oblo/modules/module_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/shared_library.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

namespace oblo
{
    namespace
    {
        module_manager* g_instance{nullptr};

        struct sorted_module
        {
            hashed_string_view id;
            u32 loadOrder;
            module_interface* ptr;
        };

        struct reverse_load_order
        {
            bool operator()(const sorted_module& lhs, const sorted_module& rhs)
            {
                return lhs.loadOrder > rhs.loadOrder;
            }
        };

        struct load_order
        {
            bool operator()(const sorted_module& lhs, const sorted_module& rhs)
            {
                return lhs.loadOrder < rhs.loadOrder;
            }
        };

        template <typename T, typename Order = load_order>
        void sort_modules(dynamic_array<sorted_module>& out, const T& modules, Order f = {})
        {
            out.reserve(modules.size());

            for (auto& [id, storage] : modules)
            {
                out.emplace_back(id, storage.loadOrder, storage.ptr.get());
            }

            std::sort(out.begin(), out.end(), f);
        }
    }

    struct module_manager::scoped_state_change
    {
        scoped_state_change(state* s, state newState) : prev{*s}, ptr{s}
        {
            *s = newState;
        }

        ~scoped_state_change()
        {
            *ptr = prev;
        }

        state prev;
        state* ptr;
    };

    struct module_manager::module_storage
    {
        unique_ptr<module_interface> ptr;
        service_registry services;
        platform::shared_library lib;
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

    module_interface* module_manager::load(cstring_view path)
    {
        platform::shared_library lib{path};

        if (!lib)
        {
            return nullptr;
        }

        void* const symbol = lib.symbol(OBLO_STRINGIZE(OBLO_MODULE_INSTANTIATE_SYM));

        if (!symbol)
        {
            return nullptr;
        }

        using module_instantiate_fn = module_interface* (*) (allocator*);

        allocator* const al = get_global_allocator();
        unique_ptr module{static_cast<module_instantiate_fn>(symbol)(al), al};
        module_interface* const ptr = module.get();

        if (!module)
        {
            return nullptr;
        }

        // It's quite important that nothing gets moved in this deque, since we give references to the strings (which
        // have SSO)
        auto& pathCopy = m_loadPaths.emplace_back(path);

        auto* const moduleEntry = load(pathCopy.as<hashed_string_view>(), std::move(module));

        if (!moduleEntry)
        {
            m_loadPaths.pop_back();
            return nullptr;
        }

        moduleEntry->lib = std::move(lib);

        return ptr;
    }

    void module_manager::finalize()
    {
        OBLO_ASSERT(m_state < state::finalizing);

        m_state = state::finalizing;

        // We finalize in load order
        dynamic_array<sorted_module> modules;
        sort_modules(modules, m_modules);

        for (auto& m : modules)
        {
            m.ptr->finalize();
        }

        m_state = state::finalized;
    }

    void module_manager::shutdown()
    {
        // We unload in reverse load order
        dynamic_array<sorted_module> modules;
        sort_modules(modules, m_modules, reverse_load_order{});

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

    module_interface* module_manager::find(const hashed_string_view& id) const
    {
        const auto it = m_modules.find(id);
        if (it == m_modules.end())
        {
            return nullptr;
        }

        return it->second.ptr.get();
    }

    module_manager::module_storage* module_manager::load(const hashed_string_view& id,
        unique_ptr<module_interface> module)
    {
        OBLO_ASSERT(m_state <= state::loading);

        scoped_state_change state{&m_state, state::loading};

        const auto [it, inserted] = m_modules.emplace(id, module_storage{});

        if (!inserted)
        {
            return nullptr;
        }

        service_registry services;

        try
        {
            if (!module->startup({
                    .services = &services,
                }))
            {
                m_modules.erase(it);
                return nullptr;
            }
        }
        catch (const std::exception&)
        {
            m_modules.erase(it);
            return nullptr;
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

        return &moduleEntry;
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