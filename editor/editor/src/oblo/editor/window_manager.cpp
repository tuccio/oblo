#include <oblo/editor/window_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/editor/window_entry.hpp>
#include <oblo/editor/window_module.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/trace/profile.hpp>

#include <bit>

#include <imgui.h>

namespace oblo::editor
{
    window_manager::window_manager() = default;

    window_manager::~window_manager()
    {
        shutdown();
    }

    window_handle window_manager::create_window_impl(window_entry* parent,
        const type_id& typeId,
        service_registry* overrideCtx,
        u8* ptr,
        update_fn update,
        destroy_fn destroy,
        std::string_view debugName)
    {
        auto* const newEntry = new (m_pool.allocate(sizeof(window_entry), alignof(window_entry))) window_entry{
            .ptr = ptr,
            .update = update,
            .destroy = destroy,
            .services = service_context{&parent->services, overrideCtx},
            .typeId = typeId,
            .debugName = debugName,
        };

        connect(parent, newEntry);

        return std::bit_cast<window_handle>(newEntry);
    }

    window_handle window_manager::find_child_impl(window_entry* parent, const type_id& type, bool recursive) const
    {
        auto* const firstChild = parent->firstChild;

        for (auto* child = firstChild; child; child = child->firstSibling)
        {
            if (child->typeId == type)
            {
                return std::bit_cast<window_handle>(child);
            }

            if (recursive)
            {
                const auto descendant = find_child_impl(child, type, true);

                if (descendant)
                {
                    return descendant;
                }
            }
        }

        return {};
    }

    void window_manager::destroy_window(window_handle handle)
    {
        auto* const window = std::bit_cast<window_entry*>(handle);

        // TODO: Not the most efficient way, but good enough for now
        while (auto* const child = window->firstChild)
        {
            destroy_window(std::bit_cast<window_handle>(child));
        }

        disconnect(window);

        if (window->destroy)
        {
            window->destroy(m_pool, window->ptr);
        }

        if (auto* const localRegistry = window->services.get_local_registry())
        {
            localRegistry->~service_registry();
            m_pool.deallocate(localRegistry, sizeof(service_registry), alignof(service_registry));
        }

        m_pool.deallocate(window, sizeof(window_entry), alignof(window_entry));
    }

    void window_manager::update()
    {
        OBLO_PROFILE_SCOPE();

        for (const auto& windowModule : m_windowModules)
        {
            windowModule->update();
        }

        update_window(m_root);
    }

    void window_manager::init()
    {
        OBLO_ASSERT(m_root == nullptr);
        m_root = new (m_pool.allocate(sizeof(window_entry), alignof(window_entry))) window_entry{};

        auto* const serviceRegistry =
            new (m_pool.allocate(sizeof(service_registry), alignof(service_registry))) service_registry{};

        m_root->services = service_context{nullptr, serviceRegistry};

        const std::span windowModuleProviders = module_manager::get().find_services<window_modules_provider>();

        for (const auto* const provider : windowModuleProviders)
        {
            provider->fetch_window_modules(m_windowModules);
        }

        for (const auto& windowModule : m_windowModules)
        {
            windowModule->init();
        }
    }

    void window_manager::shutdown()
    {
        if (m_root)
        {
            destroy_window(std::bit_cast<window_handle>(m_root));
            m_root = nullptr;
        }
    }

    service_registry& window_manager::get_global_service_registry()
    {
        return *m_root->services.get_local_registry();
    }

    window_entry* window_manager::update_window(window_entry* entry)
    {
        if (entry->update)
        {
            OBLO_PROFILE_SCOPE("Update Window");
            OBLO_PROFILE_TAG(entry->debugName);

            const auto handle = std::bit_cast<window_handle>(entry);
            const bool shouldDestroy = !entry->update(entry->ptr, make_window_update_context(handle));

            if (shouldDestroy)
            {
                auto* const next = entry->firstSibling;
                destroy_window(handle);
                return next;
            }
        }

        for (auto* next = entry->firstChild; next != nullptr;)
        {
            next = update_window(next);
        }

        for (auto* next = entry->firstSibling; next != nullptr;)
        {
            next = update_window(next);
        }

        return nullptr;
    }

    void window_manager::connect(window_entry* parent, window_entry* child)
    {
        OBLO_ASSERT(child->parent == nullptr);
        OBLO_ASSERT(child->firstChild == nullptr);
        OBLO_ASSERT(child->firstSibling == nullptr);
        OBLO_ASSERT(child->prevSibling == nullptr);

        auto* firstParentChild = parent->firstChild;

        child->parent = parent;
        child->firstSibling = firstParentChild;

        if (firstParentChild)
        {
            firstParentChild->prevSibling = child;
        }

        parent->firstChild = child;
    }

    void window_manager::disconnect(window_entry* child)
    {
        auto* parent = child->parent;

        if (!parent)
        {
            return;
        }

        auto* const firstSibling = child->firstSibling;
        auto* const prevSibling = child->prevSibling;

        if (parent->firstChild == child)
        {
            OBLO_ASSERT(!prevSibling);
            parent->firstChild = firstSibling;
        }

        if (firstSibling)
        {
            firstSibling->prevSibling = prevSibling;
        }

        if (prevSibling)
        {
            prevSibling->firstSibling = firstSibling;
        }
    }

    window_update_context window_manager::make_window_update_context(window_handle handle)
    {
        auto* const entry = reinterpret_cast<window_entry*>(handle.value);
        return {
            .windowManager = *this,
            .windowHandle = handle,
            .services = entry->services,
        };
    }

    service_registry* window_manager::create_new_registry(service_registry&& services)
    {
        return new (m_pool.allocate(sizeof(service_registry), alignof(service_registry)))
            service_registry{std::move(services)};
    }

    type_id window_manager::get_window_type(window_handle handle) const
    {
        auto* const entry = reinterpret_cast<window_entry*>(handle.value);
        return entry->typeId;
    }

    u8* window_manager::get_window_pointer(window_handle handle) const
    {
        auto* const entry = reinterpret_cast<window_entry*>(handle.value);
        return entry->ptr;
    }

    window_handle window_manager::get_parent(window_handle handle) const
    {
        auto* const entry = reinterpret_cast<window_entry*>(handle.value);
        return std::bit_cast<window_handle>(entry->parent);
    }
}