#pragma once

#include <oblo/core/intrusive_list.hpp>
#include <oblo/ecs/systems/system_descriptor.hpp>

namespace oblo::ecs
{
    struct system_trampoline;

    struct system_trampoline_userdata
    {
        system_descriptor wrapped;
        intrusive_list<system_trampoline>* list{};
    };

    struct system_trampoline
    {
        static void* create(void* userdata)
        {
            auto* const tud = reinterpret_cast<system_trampoline_userdata*>(userdata);
            auto* const self = new system_trampoline{
                .system = tud->wrapped.create(tud->wrapped.userdata),
            };

            self->patch_update_functions(tud->wrapped);
            tud->list->push_front(self);

            return self;
        }

        static void destroy(void* userdata, void* ptr)
        {
            auto* const self = reinterpret_cast<system_trampoline*>(ptr);
            auto* const tud = reinterpret_cast<system_trampoline_userdata*>(userdata);

            tud->list->remove(self);

            if (self->system)
            {
                tud->wrapped.destroy(tud->wrapped.userdata, self->system);
            }

            deallocate_userdata(userdata);
            delete self;
        }

        static void first_update(void* userdata, void* ptr, const ecs::system_update_context* ctx)
        {
            auto* const self = reinterpret_cast<system_trampoline*>(ptr);

            if (self->firstUpdateFn)
            {
                auto* const tud = reinterpret_cast<system_trampoline_userdata*>(userdata);
                self->firstUpdateFn(tud->wrapped.userdata, self->system, ctx);
            }
        }

        static void update(void* userdata, void* ptr, const ecs::system_update_context* ctx)
        {
            auto* const self = reinterpret_cast<system_trampoline*>(ptr);

            if (self->updateFn)
            {
                auto* const tud = reinterpret_cast<system_trampoline_userdata*>(userdata);
                self->updateFn(tud->wrapped.userdata, self->system, ctx);
            }
        }

        static void* allocate_userdata(const system_trampoline_userdata& userdata)
        {
            return new system_trampoline_userdata{userdata};
        }

        static void deallocate_userdata(void* ptr)
        {
            delete reinterpret_cast<system_trampoline_userdata*>(ptr);
        }

        void patch_update_functions(const system_descriptor& desc)
        {
            firstUpdateFn = desc.firstUpdate ? desc.firstUpdate : desc.update;
            updateFn = desc.update;
        }

        void* system{};
        system_trampoline* intrusiveNext{};
        system_trampoline* intrusivePrev{};
        system_update_fn firstUpdateFn{};
        system_update_fn updateFn{};
    };

    inline system_descriptor make_system_trampoline_descriptor(const system_trampoline_userdata& userdata)
    {
        return {
            .create = system_trampoline::create,
            .destroy = system_trampoline::destroy,
            .firstUpdate = system_trampoline::first_update,
            .update = system_trampoline::update,
            .userdata = system_trampoline::allocate_userdata(userdata),
        };
    }

    inline void swap_system_trampolines(const intrusive_list<system_trampoline>& list, const system_descriptor& desc)
    {
        for (system_trampoline* t = list.head; t != nullptr; t = t->intrusiveNext)
        {
            t->patch_update_functions(desc);
        }
    }
}