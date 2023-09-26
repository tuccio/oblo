#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/reflection/reflection_registry_impl.hpp>

namespace oblo::reflection
{
    template <typename T>
    u32 append_to_vector(std::vector<T>& v)
    {
        if (v.empty())
        {
            // We use index 0 as invalid handle
            v.emplace_back();
        }

        const auto classIndex = u32(v.size());

        v.emplace_back();

        return classIndex;
    }

    u32 reflection_registry::registrant::add_new_class(const type_id& type)
    {
        const auto [it, ok] = m_impl.typesMap.emplace(type, type_handle{});
        OBLO_ASSERT(ok);

        if (!ok)
        {
            return 0;
        }

        const auto typeIndex = append_to_vector(m_impl.types);
        const auto classIndex = append_to_vector(m_impl.classes);

        it->second = type_handle{typeIndex};

        auto& classData = m_impl.classes.back();
        classData.type = type;

        auto& typeData = m_impl.types.back();
        typeData.type = type;
        typeData.concreteIndex = classIndex;
        typeData.kind = type_kind::class_kind;

        return classIndex;
    }

    void reflection_registry::registrant::add_field(u32 classIndex,
                                                    const type_id& type,
                                                    std::string_view name,
                                                    u32 offset)
    {
        m_impl.classes[classIndex].fields.emplace_back(field_data{
            .type = type,
            .name = name,
            .offset = offset,
        });
    }
}