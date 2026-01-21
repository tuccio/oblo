#include <oblo/script/behaviour/script_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/script/behaviour/options.hpp>
#include <oblo/script/behaviour/script_behaviour_component.hpp>
#include <oblo/script/interpreter.hpp>
#include <oblo/script/resources/builtin_api.hpp>
#include <oblo/script/resources/compiled_script.hpp>

namespace oblo
{
    namespace
    {
        constexpr u32 g_StackSize = 4096;

        struct script_api_context
        {
            time currentTime;
            ecs::entity entityId;
        };
    }

    class script_behaviour_system::script_api_impl
    {
    public:
        bool init(const property_registry& propertyRegistry, ecs::entity_registry& entities)
        {
            m_propertyRegistry = &propertyRegistry;
            m_entities = &entities;

            return true;
        }

        bool is_initialized() const
        {
            return m_propertyRegistry != nullptr;
        }

        void register_api_functions(interpreter& i)
        {
            i.register_api(
                script_api::get_time,
                [this](interpreter& interp) { return get_time_impl(interp); },
                0,
                sizeof(f32));

            i.register_api(
                script_api::ecs::set_property_f32,
                [this](interpreter& interp) { return ecs_set_property_impl<f32>(interp); },
                script_string_ref_size() * 2 + sizeof(f32),
                0);

            i.register_api(
                script_api::ecs::get_property_f32,
                [this](interpreter& interp) { return ecs_get_property_impl<f32>(interp); },
                script_string_ref_size() * 2,
                sizeof(f32));

            i.register_api(
                script_api::ecs::set_property_vec3,
                [this](interpreter& interp) { return ecs_set_property_vec3(interp); },
                script_string_ref_size() * 2 + sizeof(u32) + 3 * sizeof(f32),
                0);
        }

        script_api_context& global_context()
        {
            return m_ctx;
        }

        f32 get_property_f32(string_view componentType, string_view propertyName)
        {
            const oblo::property* propertyData{};

            const auto p = fetch_component_property_ptr(componentType, propertyName, &propertyData);
            OBLO_ASSERT(p);

            OBLO_ASSERT(propertyData->kind == property_kind::f32);

            if (!p || propertyData->kind != property_kind::f32) [[unlikely]]
            {
                return 0.f;
            }

            return *reinterpret_cast<const f32*>(*p);
        }

        void set_property_f32(string_view componentType, string_view propertyName, f32 value)
        {
            const oblo::property* propertyData{};

            const auto p = fetch_component_property_ptr(componentType, propertyName, &propertyData);
            OBLO_ASSERT(p);

            OBLO_ASSERT(propertyData->kind == property_kind::f32);

            if (!p || propertyData->kind != property_kind::f32) [[unlikely]]
            {
                return;
            }

            std::memcpy(*p, &value, sizeof(f32));
            m_entities->notify(m_ctx.entityId);
        }

        vec3 get_property_vec3(string_view componentType, string_view propertyName)
        {
            const oblo::property_node* propertyData{};

            const expected propertyPtr = fetch_component_property_node_ptr(componentType, propertyName, &propertyData);

            if (!propertyPtr || propertyData->type != get_type_id<vec3>()) [[unlikely]]
            {
                return vec3{};
            }

            return *reinterpret_cast<const vec3*>(*propertyPtr);
        }

        void set_property_vec3(string_view componentType, string_view propertyName, u32 valuesMask, vec3 inData)
        {
            const oblo::property_node* propertyData{};

            const expected propertyPtr = fetch_component_property_node_ptr(componentType, propertyName, &propertyData);

            if (!propertyPtr || propertyData->type != get_type_id<vec3>()) [[unlikely]]
            {
                return;
            }

            if (valuesMask != 0)
            {
                for (u32 i = 0, iMask = 1; i < 3; ++i, iMask <<= 1)
                {
                    if ((iMask & valuesMask) == 0)
                    {
                        continue;
                    }

                    byte* const dst = *propertyPtr + i * sizeof(f32);
                    const f32* const src = &inData[i];

                    std::memcpy(dst, src, sizeof(f32));
                }

                m_entities->notify(m_ctx.entityId);
            }

            return;
        }

    private:
        expected<void, interpreter_error> get_time_impl(interpreter& interp)
        {
            const f32 t = to_f32_seconds(m_ctx.currentTime);
            return interp.set_function_return(as_bytes(std::span{&t, 1}));
        }

        template <typename T>
        expected<void, interpreter_error> ecs_set_property_impl(interpreter& interp)
        {
            const expected componentType = interp.get_string_view(0);
            const expected property = interp.get_string_view(script_string_ref_size());

            if (!componentType || !property)
            {
                return interpreter_error::invalid_arguments;
            }

            T inData;

            if constexpr (sizeof(T) == sizeof(u32))
            {
                const expected data = interp.read_u32(2 * script_string_ref_size());

                if (!data)
                {
                    return data.error();
                }

                std::memcpy(&inData, &data.value(), sizeof(u32));
            }
            else
            {
                OBLO_ASSERT(false);
                return interpreter_error::invalid_arguments;
            }

            const oblo::property* propertyData{};
            const expected propertyPtr = fetch_component_property_ptr(*componentType, *property, &propertyData);

            if (!propertyPtr)
            {
                return propertyPtr.error();
            }

            property_value_wrapper w;
            w.assign_from(propertyData->kind, &inData);
            w.assign_to(propertyData->kind, *propertyPtr);

            m_entities->notify(m_ctx.entityId);

            return no_error;
        }

        expected<void, interpreter_error> ecs_set_property_vec3(interpreter& interp)
        {
            const expected componentType = interp.get_string_view(0);
            const expected property = interp.get_string_view(script_string_ref_size());
            const expected mask = interp.read_u32(2 * script_string_ref_size());

            if (!componentType || !property)
            {
                return interpreter_error::invalid_arguments;
            }

            const u32 valueOffset = u32(2 * script_string_ref_size() + sizeof(u32));

            const expected<f32, interpreter_error> inData[3] = {
                interp.read_f32(valueOffset),
                interp.read_f32(valueOffset + sizeof(f32)),
                interp.read_f32(valueOffset + sizeof(f32) * 2),
            };

            for (u32 i = 0; i < 3; ++i)
            {
                if (!inData[i])
                {
                    return inData[i].error();
                }
            }

            const property_node* propertyData{};
            const expected propertyPtr = fetch_component_property_node_ptr(*componentType, *property, &propertyData);

            if (!propertyPtr)
            {
                return propertyPtr.error();
            }

            if (propertyData->type != get_type_id<vec3>())
            {
                return interpreter_error::invalid_arguments;
            }

            const u32 valuesMask = *mask;

            if (valuesMask != 0)
            {
                for (u32 i = 0, iMask = 1; i < 3; ++i, iMask <<= 1)
                {
                    if ((iMask & valuesMask) == 0)
                    {
                        continue;
                    }

                    byte* const dst = *propertyPtr + i * sizeof(f32);
                    const f32* const src = &*inData[i];

                    std::memcpy(dst, src, sizeof(f32));
                }

                m_entities->notify(m_ctx.entityId);
            }

            return no_error;
        }

        template <typename T>
        expected<void, interpreter_error> ecs_get_property_impl(interpreter& interp)
        {
            const expected componentType = interp.get_string_view(0);
            const expected property = interp.get_string_view(script_string_ref_size());

            if (!componentType || !property)
            {
                return interpreter_error::invalid_arguments;
            }

            const expected propertyPtr = fetch_component_property_ptr(*componentType, *property);

            if (!propertyPtr)
            {
                return propertyPtr.error();
            }

            return interp.set_function_return({*propertyPtr, sizeof(T)});
        }

        OBLO_FORCEINLINE expected<byte*, interpreter_error> fetch_component_property_ptr(
            string_view componentType, string_view property, const oblo::property** outProperty = nullptr)
        {
            const auto entry = get_or_add_to_cache(hashed_string_view{componentType},
                property,
                m_entities->get_type_registry(),
                property_entry_kind::property);

            if (!entry.tree || entry.kind != property_entry_kind::property ||
                entry.index >= entry.tree->properties.size()) [[unlikely]]
            {
                log::error("Failed to locate property {}::{}", componentType, property);
                return interpreter_error::invalid_arguments;
            }

            const auto& propertyData = entry.tree->properties[entry.index];

            if (propertyData.kind == property_kind::string)
            {
                log::error("Unsupported property type {}::{}", componentType, property);
                return interpreter_error::invalid_arguments;
            }

            byte* componentPtr[1];
            const ecs::component_type types[1] = {entry.componentId};
            m_entities->get(m_ctx.entityId, types, componentPtr);

            if (!componentPtr[0])
            {
                log::debug("Entity {} has no component {}", m_ctx.entityId.value, componentType);
                return interpreter_error::invalid_arguments;
            }

            if (outProperty)
            {
                *outProperty = &propertyData;
            }

            return componentPtr[0] + propertyData.offset;
        }

        OBLO_FORCEINLINE expected<byte*, interpreter_error> fetch_component_property_node_ptr(
            string_view componentType, string_view property, const property_node** outPropertyNode = nullptr)
        {
            const auto entry = get_or_add_to_cache(hashed_string_view{componentType},
                property,
                m_entities->get_type_registry(),
                property_entry_kind::property_node);

            if (!entry.tree || entry.kind != property_entry_kind::property_node ||
                entry.index >= entry.tree->nodes.size()) [[unlikely]]
            {
                log::error("Failed to locate property {}::{}", componentType, property);
                return interpreter_error::invalid_arguments;
            }

            const auto& propertyData = entry.tree->nodes[entry.index];

            byte* componentPtr[1];
            const ecs::component_type types[1] = {entry.componentId};
            m_entities->get(m_ctx.entityId, types, componentPtr);

            if (!componentPtr[0])
            {
                log::debug("Entity {} has no component {}", m_ctx.entityId.value, componentType);
                return interpreter_error::invalid_arguments;
            }

            if (outPropertyNode)
            {
                *outPropertyNode = &propertyData;
            }

            return componentPtr[0] + propertyData.offset;
        }

    private:
        using property_hash = usize;

        enum class property_entry_kind : u8
        {
            none,
            property,
            property_node,
            enum_max,
        };

        struct property_entry
        {
            const property_tree* tree;
            ecs::component_type componentId;
            u32 index;
            property_entry_kind kind;
        };

    private:
        property_entry get_or_add_to_cache(hashed_string_view typeName,
            string_view property,
            const ecs::type_registry& types,
            flags<property_entry_kind> searchFlags)
        {
            const property_hash propertyHash = hash_all<hash>(typeName, property);

            const auto [it, inserted] = m_componentProperties.emplace(propertyHash, property_entry{});

            if (!inserted)
            {
                return it->second;
            }

            const type_id typeId{.name = typeName};
            auto* const tree = m_propertyRegistry->try_get(typeId);

            // Cache the result pointer since the map is stable and iterators will be invalidated
            auto& result = it->second;

            // Let's cache all properties for the component, this will invalidate the map iterator
            if (tree)
            {
                const auto componentId = types.find_component(typeId);

                string_builder builder;

                if (searchFlags.contains(property_entry_kind::property))
                {
                    for (u32 propertyIdx = 0; propertyIdx < tree->properties.size32(); ++propertyIdx)
                    {
                        builder.clear();
                        create_property_path(builder, *tree, tree->properties[propertyIdx]);

                        const property_hash newPropertyHash = hash_all<hash>(typeId, builder);

                        auto& newProperty = m_componentProperties[newPropertyHash];

                        if (newProperty.tree) [[unlikely]]
                        {
                            log::error("A hash conflict between properties was detected");
                            continue;
                        }

                        newProperty = property_entry{
                            .tree = tree,
                            .componentId = componentId,
                            .index = propertyIdx,
                            .kind = property_entry_kind::property,
                        };
                    }
                }

                if (searchFlags.contains(property_entry_kind::property_node))
                {
                    for (u32 propertyNodeIdx = 0; propertyNodeIdx < tree->nodes.size32(); ++propertyNodeIdx)
                    {
                        builder.clear();
                        create_property_path(builder, *tree, tree->nodes[propertyNodeIdx]);

                        const property_hash newPropertyHash = hash_all<hash>(typeId, builder);

                        auto& newProperty = m_componentProperties[newPropertyHash];

                        if (newProperty.tree) [[unlikely]]
                        {
                            log::error("A hash conflict between properties was detected");
                            continue;
                        }

                        newProperty = property_entry{
                            .tree = tree,
                            .componentId = componentId,
                            .index = propertyNodeIdx,
                            .kind = property_entry_kind::property_node,
                        };
                    }
                }
            }

            return result;
        }

    private:
        script_api_context m_ctx{};
        const property_registry* m_propertyRegistry{};
        ecs::entity_registry* m_entities{};
        std::unordered_map<property_hash, property_entry> m_componentProperties;
    };

    script_behaviour_system::script_behaviour_system() = default;
    script_behaviour_system::~script_behaviour_system() = default;

    void script_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        option_proxy<script_options::use_native_runtime> useNativeRuntimeProxy;

        if (auto* optsModule = module_manager::get().find<options_module>(); optsModule)
        {
            auto& optsManager = optsModule->manager();
            useNativeRuntimeProxy.init(optsManager);
            useNativeRuntime = useNativeRuntimeProxy.read(optsManager);
        }
        else
        {
            log::debug("Failed to retrieve options manager, " OBLO_STRINGIZE(
                script_behaviour_system) " might be misconfigured");

            useNativeRuntime = useNativeRuntimeProxy.descriptor().defaultValue.get_bool();
        }

        m_scriptApi = allocate_unique<script_api_impl>();

        auto* const propertyRegistry = ctx.services->find<const property_registry>();

        if (!propertyRegistry || !m_scriptApi->init(*propertyRegistry, *ctx.entities))
        {
            log::error("Failed to initialized script API");
        }

        m_resourceRegistry = ctx.services->find<const resource_registry>();
        update(ctx);
    }

    void script_behaviour_system::update(const ecs::system_update_context& ctx)
    {
        if (!m_scriptApi->is_initialized()) [[unlikely]]
        {
            return;
        }

        ecs::deferred deferred;

        for (auto&& chunk :
            ctx.entities->range<script_behaviour_component>().exclude<script_behaviour_state_component>())
        {
            for (auto&& [e, b] : chunk.zip<ecs::entity, script_behaviour_component>())
            {
                auto& state = deferred.add<script_behaviour_state_component>(e);

                state.script = m_resourceRegistry->get_resource(b.script);

                if (!b.script)
                {
                    continue;
                }

                if (!state.script.is_loaded())
                {
                    state.script.load_start_async();
                    continue;
                }
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<script_behaviour_component, script_behaviour_state_component>())
        {
            for (auto&& [e, b, state] :
                chunk.zip<ecs::entity, script_behaviour_component, script_behaviour_state_component>())
            {
                if (state.script.is_invalidated() || state.bytecode.is_invalidated() ||
                    state.script.as_ref() != b.script)
                {
                    deferred.remove<script_behaviour_state_component, script_behaviour_update_tag>(e);
                    continue;
                }

                if (!useNativeRuntime)
                {
                    if (!state.runtime && state.script.is_loaded())
                    {
                        if (!state.bytecode)
                        {
                            auto& scriptInfo = *state.script;
                            state.bytecode = m_resourceRegistry->get_resource(scriptInfo.bytecode);

                            if (!state.bytecode.is_loaded())
                            {
                                state.bytecode.load_start_async();
                                continue;
                            }
                        }
                        else if (!state.bytecode.is_loaded())
                        {
                            continue;
                        }

                        // TOOD: Every script owns the stack here, we would like to share memory instead
                        state.runtime = allocate_unique<interpreter>();
                        state.runtime->init(g_StackSize);
                        state.runtime->load_module(state.bytecode->module);

                        m_scriptApi->register_api_functions(*state.runtime);

                        // Assume the whole module is an update function for now
                        // TODO: Handle multiple entry points (e.g. init, update)
                        deferred.add<script_behaviour_update_tag>(e);
                    }
                }
                else
                {
                    // TODO: Check if scripts are invalidated, if so reload.

                    if (ctx.entities->has<script_behaviour_update_tag>(e))
                    {
                        continue;
                    }

                    if (!state.native)
                    {
                        if (!state.script.is_loaded())
                        {
                            state.script.load_start_async();
                            continue;
                        }

                        state.native = m_resourceRegistry->get_resource(state.script->x86_64_sse2);

                        if (state.native && !state.native.is_loaded())
                        {
                            state.native.load_start_async();
                            continue;
                        }
                    }
                    else if (state.native->module.is_open())
                    {
                        using loader_fn = void* (*) (const char*);

                        constexpr loader_fn loader = [](const char* name) -> void*
                        {
                            const hashed_string_view hName{name};

                            if (hName == script_api::cosine_f32)
                            {
                                return +[](script_api_impl*, f32 v) { return std::cos(v); };
                            }

                            if (hName == script_api::sine_vec3)
                            {
                                return +[](script_api_impl*, vec3 v)
                                {
                                    return vec3{
                                        std::cos(v.x),
                                        std::cos(v.y),
                                        std::cos(v.z),
                                    };
                                };
                            }

                            if (hName == script_api::sine_f32)
                            {
                                return +[](script_api_impl*, f32 v) { return std::cos(v); };
                            }

                            if (hName == script_api::sine_vec3)
                            {
                                return +[](script_api_impl*, vec3 v)
                                {
                                    return vec3{
                                        std::sin(v.x),
                                        std::sin(v.y),
                                        std::sin(v.z),
                                    };
                                };
                            }

                            if (hName == script_api::get_time)
                            {
                                return +[](script_api_impl* ctx) -> f32
                                { return to_f32_seconds(ctx->global_context().currentTime); };
                            }

                            if (hName == script_api::ecs::get_property_f32)
                            {
                                return +[](script_api_impl* api,
                                            const char* componentType,
                                            const char* propertyName) -> f32
                                { return api->get_property_f32(componentType, propertyName); };
                            }

                            if (hName == script_api::ecs::get_property_vec3)
                            {
                                return +[](script_api_impl* api,
                                            const char* componentType,
                                            const char* propertyName) -> vec3
                                { return api->get_property_vec3(componentType, propertyName); };
                            }

                            if (hName == script_api::ecs::set_property_f32)
                            {
                                return +[](script_api_impl* api,
                                            const char* componentType,
                                            const char* propertyName,
                                            f32 value) -> void
                                { api->set_property_f32(componentType, propertyName, value); };
                            }

                            if (hName == script_api::ecs::set_property_vec3)
                            {
                                return +[](script_api_impl* api,
                                            const char* componentType,
                                            const char* propertyName,
                                            u32 mask,
                                            vec3 value) -> void
                                { api->set_property_vec3(componentType, propertyName, mask, value); };
                            }

                            return nullptr;
                        };

                        const auto loadSymbols =
                            reinterpret_cast<i32 (*)(loader_fn)>(state.native->module.symbol("oblo_load_symbols"));

                        const auto setContext =
                            reinterpret_cast<void (*)(void*)>(state.native->module.symbol("oblo_set_global_context"));

                        const auto execute =
                            reinterpret_cast<void (*)()>(state.native->module.symbol("node_graph_execute"));

                        if (loadSymbols && setContext && execute && loadSymbols(loader))
                        {
                            state.setGlobalContext = setContext;
                            state.execute = execute;
                            deferred.add<script_behaviour_update_tag>(e);
                        }
                    }
                }
            }
        }

        deferred.apply(*ctx.entities);

        auto& apiCtx = m_scriptApi->global_context();

        for (auto&& chunk : ctx.entities->range<script_behaviour_state_component>().with<script_behaviour_update_tag>())
        {
            for (auto&& [e, state] : chunk.zip<ecs::entity, script_behaviour_state_component>())
            {
                apiCtx.entityId = e;

                if (!useNativeRuntime)
                {
                    if (!state.runtime->run())
                    {
                        log::debug("Script execution failed for entity {}", e.value);
                    }

                    state.runtime->reset_execution();
                }
                else
                {
                    state.setGlobalContext(&apiCtx);
                    state.execute();
                }
            }
        }

        apiCtx.currentTime = apiCtx.currentTime + ctx.dt;
    }

    void script_behaviour_system::shutdown() {}
}