#include <oblo/ecs/systems/system_graph.hpp>

#include <oblo/core/expected.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/ecs/forward.hpp>

#include <span>
#include <unordered_map>

namespace oblo::ecs
{
    class system_graph_builder
    {
    public:
        class barrier_builder;

    public:
        system_graph_builder();
        system_graph_builder(const system_graph_builder&) = delete;
        system_graph_builder(system_graph_builder&&) = delete;

        ~system_graph_builder();

        system_graph_builder& operator=(const system_graph_builder&) = delete;
        system_graph_builder& operator=(system_graph_builder&&) = delete;

        barrier_builder add_system(const system_descriptor& desc,
            std::span<const type_id> before,
            const std::span<const type_id> after,
            std::span<const type_id> barriers);

        template <typename T>
        barrier_builder add_system();

        template <typename T>
        barrier_builder add_barrier();

        expected<system_graph> build();

    private:
        struct barrier_systems;

    private:
        barrier_systems& get_or_add(const type_id& typeId);

    private:
        system_graph m_graph;
        std::unordered_map<type_id, barrier_systems> m_barrier;
    };

    class system_graph_builder::barrier_builder
    {
    public:
        template <typename B>
        const barrier_builder& after() const;

        template <typename B>
        const barrier_builder& before() const;

        template <typename T>
        const barrier_builder& as() const;

        const barrier_builder& after(const type_id& type) const;
        const barrier_builder& before(const type_id& type) const;
        const barrier_builder& as(const type_id& type) const;

    private:
        friend class system_graph_builder;

    private:
        system_graph_builder* m_builder{};
        type_id m_typeId{};
        h32<system> m_system{};
    };

    template <typename T>
    system_graph_builder::barrier_builder system_graph_builder::add_system()
    {
        return add_system(make_system_descriptor<T>(), {}, {}, {});
    }

    template <typename T>
    system_graph_builder::barrier_builder system_graph_builder::add_barrier()
    {
        constexpr auto typeId = get_type_id<T>();

        return add_system(
            {
                .name = typeId.name,
                .typeId = typeId,
            },
            {},
            {},
            {&typeId, 1});
    }

    template <typename B>
    const system_graph_builder::barrier_builder& system_graph_builder::barrier_builder::after() const
    {
        return after(get_type_id<B>());
    }

    template <typename B>
    const system_graph_builder::barrier_builder& system_graph_builder::barrier_builder::before() const
    {
        return before(get_type_id<B>());
    }

    template <typename T>
    const system_graph_builder::barrier_builder& system_graph_builder::barrier_builder::as() const
    {
        return as(get_type_id<T>());
    }
}