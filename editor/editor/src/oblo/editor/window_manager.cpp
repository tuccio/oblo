#include <oblo/editor/window_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/editor/window_entry.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <bit>

namespace oblo::editor
{
    window_manager::~window_manager()
    {
        shutdown();
    }

    window_handle window_manager::create_window_impl(
        window_entry* parent, service_registry* overrideCtx, u8* ptr, update_fn update, destroy_fn destroy)
    {
        auto* const newEntry = new (m_pool.allocate(sizeof(window_entry), alignof(window_entry))) window_entry{
            .ptr = ptr,
            .update = update,
            .destroy = destroy,
            .services = overrideCtx ? overrideCtx : parent->services,
            .isServiceRegistryOwned = overrideCtx != nullptr,
        };

        connect(parent, newEntry);

        return std::bit_cast<window_handle>(newEntry);
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

        if (window->isServiceRegistryOwned)
        {
            window->services->~service_registry();
            m_pool.deallocate(window->services, sizeof(service_registry), alignof(service_registry));
        }

        m_pool.deallocate(window, sizeof(window_entry), alignof(window_entry));
    }

    void window_manager::update()
    {
        update_window(m_root);
    }

    void window_manager::init()
    {
        OBLO_ASSERT(m_root == nullptr);
        m_root = new (m_pool.allocate(sizeof(window_entry), alignof(window_entry))) window_entry{};
    }

    void window_manager::shutdown()
    {
        if (m_root)
        {
            destroy_window(std::bit_cast<window_handle>(m_root));
            m_root = nullptr;
        }
    }

    window_entry* window_manager::update_window(window_entry* entry)
    {
        if (entry->update)
        {
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
    window_update_context window_manager::make_window_update_context(window_handle handle) const
    {
        auto* const entry = reinterpret_cast<window_entry*>(handle.value);
        return {.services = *entry->services};
    }

}