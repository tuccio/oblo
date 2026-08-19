#include <oblo/script/behaviour/script_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/script/behaviour/events.hpp>
#include <oblo/script/behaviour/script_behaviour_component.hpp>
#include <oblo/script/resources/builtin_api.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/reflection_script_api.hpp>

#include <type_traits>
#include <utility>

namespace oblo
{
    namespace
    {
        struct script_api_context
        {
            time currentTime;
            ecs::entity entityId;
        };

        struct script_function_entry
        {
            void* fn{};
            reflection::invoker_fn invoker{};
            u32 paramCount{};
        };
    }

    class script_behaviour_system::script_api_impl
    {
    public:
        bool init(const property_registry& propertyRegistry, ecs::entity_registry& entities)
        {
            m_propertyRegistry = &propertyRegistry;
            m_entities = &entities;

            const reflection::reflection_registry& reflectionRegistry = m_propertyRegistry->get_reflection_registry();

            script_api::for_each_script_function(reflectionRegistry,
                [this](const reflection::function_data& data)
                {
                    if (!data.invoker || !data.functionPtr)
                    {
                        return true;
                    }

                    m_scriptFunctions.emplace(hashed_string_view{data.fullyQualifiedName},
                        script_function_entry{
                            .fn = data.functionPtr,
                            .invoker = data.invoker,
                            .paramCount = data.parameterTypes.size32(),
                        });

                    return true;
                });

            return true;
        }

        bool is_initialized() const
        {
            return m_propertyRegistry != nullptr;
        }

        script_api_context& global_context()
        {
            return m_ctx;
        }

        bool load_native_module(script_behaviour_state_component& state)
        {
            using loader_fn = void* (*) (const char*);

            constexpr loader_fn loader = [](const char* name) -> void*
            {
                const hashed_string_view hName{name};

                if (hName == script_api::cosine_f32)
                {
                    constexpr auto cosine_f32 = [](script_api_impl*, f32 v) { return std::cos(v); };
                    return reinterpret_cast<void*>(+cosine_f32);
                }

                if (hName == script_api::cosine_vec3)
                {
                    constexpr auto cosine_vec3 = [](script_api_impl*, vec3 v)
                    {
                        return vec3{
                            std::cos(v.x),
                            std::cos(v.y),
                            std::cos(v.z),
                        };
                    };

                    return reinterpret_cast<void*>(+cosine_vec3);
                }

                if (hName == script_api::sine_f32)
                {
                    constexpr auto sine_f32 = [](script_api_impl*, f32 v) { return std::sin(v); };
                    return reinterpret_cast<void*>(+sine_f32);
                }

                if (hName == script_api::sine_vec3)
                {
                    constexpr auto sine_vec3 = [](script_api_impl*, vec3 v)
                    {
                        return vec3{
                            std::sin(v.x),
                            std::sin(v.y),
                            std::sin(v.z),
                        };
                    };

                    return reinterpret_cast<void*>(+sine_vec3);
                }

                if (hName == script_api::get_time)
                {
                    constexpr auto get_time = [](script_api_impl* ctx) -> f32
                    { return to_f32_seconds(ctx->global_context().currentTime); };

                    return reinterpret_cast<void*>(+get_time);
                }

                if (hName == script_api::ecs::get_property_f32)
                {
                    constexpr auto get_property_f32 =
                        [](script_api_impl* api, const char* componentType, const char* propertyName) -> f32
                    { return api->get_property_f32(componentType, propertyName); };

                    return reinterpret_cast<void*>(+get_property_f32);
                }

                if (hName == script_api::ecs::get_property_vec3)
                {
                    constexpr auto get_property_vec3 =
                        +[](script_api_impl* api, const char* componentType, const char* propertyName) -> vec3
                    { return api->get_property_vec3(componentType, propertyName); };

                    return reinterpret_cast<void*>(+get_property_vec3);
                }

                if (hName == script_api::ecs::set_property_f32)
                {
                    constexpr auto set_property_f32 =
                        [](script_api_impl* api, const char* componentType, const char* propertyName, f32 value) -> void
                    { api->set_property_f32(componentType, propertyName, value); };

                    return reinterpret_cast<void*>(+set_property_f32);
                }

                if (hName == script_api::ecs::set_property_vec3)
                {
                    constexpr auto set_property_vec3 = [](script_api_impl* api,
                                                           const char* componentType,
                                                           const char* propertyName,
                                                           u32 mask,
                                                           vec3 value) -> void
                    { api->set_property_vec3(componentType, propertyName, mask, value); };

                    return reinterpret_cast<void*>(+set_property_vec3);
                }

                if (hName == script_api::invoke_reflected_function_void)
                {
                    constexpr auto invoke_reflected_function_void =
                        [](script_api_impl* api, const char* name, u32 count, void* const* args) -> void
                    { api->invoke_reflected_function(name, nullptr, count, args); };

                    return reinterpret_cast<void*>(+invoke_reflected_function_void);
                }

                if (hName == script_api::invoke_reflected_function_i32)
                {
                    constexpr auto invoke_reflected_function_i32 =
                        [](script_api_impl* api, const char* name, u32 count, void* const* args) -> i32
                    {
                        i32 result{};
                        api->invoke_reflected_function(name, &result, count, args);
                        return result;
                    };

                    return reinterpret_cast<void*>(+invoke_reflected_function_i32);
                }

                if (hName == script_api::invoke_reflected_function_f32)
                {
                    constexpr auto invoke_reflected_function_f32 =
                        [](script_api_impl* api, const char* name, u32 count, void* const* args) -> f32
                    {
                        f32 result{};
                        api->invoke_reflected_function(name, &result, count, args);
                        return result;
                    };

                    return reinterpret_cast<void*>(+invoke_reflected_function_f32);
                }

                if (hName == script_api::invoke_reflected_function_vec3)
                {
                    constexpr auto invoke_reflected_function_vec3 =
                        [](script_api_impl* api, const char* name, u32 count, void* const* args) -> vec3
                    {
                        vec3 result{};
                        api->invoke_reflected_function(name, &result, count, args);
                        return result;
                    };

                    return reinterpret_cast<void*>(+invoke_reflected_function_vec3);
                }

                if (hName == script_api::invoke_reflected_function_bool)
                {
                    constexpr auto invoke_reflected_function_bool =
                        [](script_api_impl* api, const char* name, u32 count, void* const* args) -> bool
                    {
                        bool result{};
                        api->invoke_reflected_function(name, &result, count, args);
                        return result;
                    };

                    return reinterpret_cast<void*>(+invoke_reflected_function_bool);
                }

                return nullptr;
            };

            const auto loadSymbols =
                reinterpret_cast<i32 (*)(loader_fn)>(state.native->module.symbol("oblo_load_symbols"));

            const auto setContext =
                reinterpret_cast<void (*)(void*)>(state.native->module.symbol("oblo_set_global_context"));

            const bool wasInitialized = loadSymbols && setContext && loadSymbols(loader);

            if (wasInitialized)
            {
                state.setGlobalContext = setContext;

                string_builder builder;

                for (const auto [member, id] : {
                         pair{
                             &script_behaviour_state_component::spawnFn,
                             string_view{"1f176cb6-ffb4-4d53-b8d2-15b510f42094"},
                         },
                         pair{
                             &script_behaviour_state_component::updateFn,
                             string_view{"dc6777ec-97c3-4c7e-8797-4d5b325a9c1c"},
                         },
                     })
                {

                    builder.clear().append("oblo_node_graph_fn_");

                    for (const char c : id)
                    {
                        const char r = std::isalnum(c) ? c : '_';
                        builder.append(r);
                    }

                    (state.*member) = reinterpret_cast<script_behaviour_state_component::execute_fn>(
                        state.native->module.symbol(builder.c_str()));
                }
            }

            return wasInitialized;
        }

    private:
        f32 get_property_f32(string_view componentType, string_view propertyName)
        {
            const oblo::property* propertyData{};

            const auto p = fetch_component_property_ptr(componentType, propertyName, &propertyData);

            if (!p || propertyData->kind != property_kind::f32) [[unlikely]]
            {
                OBLO_ASSERT_ONCE(false);
                return 0.f;
            }

            return *reinterpret_cast<const f32*>(*p);
        }

        void set_property_f32(string_view componentType, string_view propertyName, f32 value)
        {
            const oblo::property* propertyData{};

            const auto p = fetch_component_property_ptr(componentType, propertyName, &propertyData);

            if (!p || propertyData->kind != property_kind::f32) [[unlikely]]
            {
                OBLO_ASSERT_ONCE(false);
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
                OBLO_ASSERT_ONCE(false);
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
                OBLO_ASSERT_ONCE(false);
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

        void invoke_reflected_function(string_view name, void* out, u32 count, void* const* args)
        {
            const auto it = m_scriptFunctions.find(name);

            if (it == m_scriptFunctions.end() || !it->second.invoker) [[unlikely]]
            {
                OBLO_ASSERT_ONCE(false);
                log::error("Failed to invoke reflected function {}", name);
                return;
            }

            const auto& entry = it->second;

            if (count != entry.paramCount) [[unlikely]]
            {
                OBLO_ASSERT_ONCE(false);
                log::error("Reflected function {} expected {} parameters, got {}", name, entry.paramCount, count);
                return;
            }

            entry.invoker(entry.fn, out, args);
        }

        OBLO_FORCEINLINE expected<byte*> fetch_component_property_ptr(
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
                return "Invalid argument"_err;
            }

            const auto& propertyData = entry.tree->properties[entry.index];

            if (propertyData.kind == property_kind::string)
            {
                log::error("Unsupported property type {}::{}", componentType, property);
                return "Invalid argument"_err;
            }

            byte* componentPtr[1];
            const ecs::component_type types[1] = {entry.componentId};
            m_entities->get(m_ctx.entityId, types, componentPtr);

            if (!componentPtr[0])
            {
                log::debug("Entity {} has no component {}", m_ctx.entityId.value, componentType);
                return "Missing component"_err;
            }

            if (outProperty)
            {
                *outProperty = &propertyData;
            }

            return componentPtr[0] + propertyData.offset;
        }

        OBLO_FORCEINLINE expected<byte*> fetch_component_property_node_ptr(
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
                return "No such property"_err;
            }

            const auto& propertyData = entry.tree->nodes[entry.index];

            byte* componentPtr[1];
            const ecs::component_type types[1] = {entry.componentId};
            m_entities->get(m_ctx.entityId, types, componentPtr);

            if (!componentPtr[0])
            {
                log::debug("Entity {} has no component {}", m_ctx.entityId.value, componentType);
                return "No such property"_err;
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

        using functions_map =
            std::unordered_map<hashed_string_view, script_function_entry, transparent_string_hash, std::equal_to<>>;

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
        functions_map m_scriptFunctions;
    };

    script_behaviour_system::script_behaviour_system() = default;
    script_behaviour_system::~script_behaviour_system() = default;

    void script_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
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

                state.script.load_start_async();
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<script_behaviour_component, script_behaviour_state_component>())
        {
            for (auto&& [e, b, state] :
                chunk.zip<ecs::entity, script_behaviour_component, script_behaviour_state_component>())
            {
                if (state.script.is_invalidated() || state.native.is_invalidated() || state.script.as_ref() != b.script)
                {
                    deferred.remove<script_behaviour_state_component, script_behaviour_update_tag>(e);
                    continue;
                }

                if (ctx.entities->has<script_behaviour_update_tag>(e))
                {
                    continue;
                }

                if (!state.native)
                {
                    if (!state.script.is_successfully_loaded())
                    {
                        state.script.load_start_async();
                        continue;
                    }

                    if constexpr (platform::is_x86_64() && platform::is_avx2())
                    {
                        state.native = m_resourceRegistry->get_resource(state.script->x86_64_avx2);
                    }

                    if (state.native && !state.native.is_successfully_loaded())
                    {
                        state.native.load_start_async();
                        continue;
                    }

                    if (!state.native)
                    {
                        log::error("No compiled native code for script {}", state.script.get_id());
                    }
                }
                else if (state.native.is_currently_loading())
                {
                    // Still loading, keep waiting
                    continue;
                }
                else if (state.native.is_successfully_loaded() && state.native->module.is_open())
                {
                    // Loaded, we can set up the state
                    if (m_scriptApi->load_native_module(state))
                    {
                        deferred.add<script_behaviour_update_tag>(e);
                        continue;
                    }
                    else
                    {
                        log::error("Failed to initialize native module for script {}", state.script.get_id());
                    }
                }
                else
                {
                    log::error("Failed to load native binary for script {}", state.script.get_id());
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

                state.setGlobalContext(&apiCtx);

                if (state.spawnFn)
                {
                    state.spawnFn();
                    state.spawnFn = {};
                }

                if (state.updateFn)
                {
                    state.updateFn();
                }
            }
        }

        apiCtx.currentTime = apiCtx.currentTime + ctx.dt;
    }

    void script_behaviour_system::shutdown() {}
}