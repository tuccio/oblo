#include <oblo/scene/serialization/model_file.hpp>
#include <oblo/scene/serialization/skeleton_file.hpp>

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/helpers.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/skeleton.hpp>

namespace oblo
{
    namespace
    {
        template <typename T>
        void write_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, const dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.child_array(parent, name);

            for (const auto& ref : array)
            {
                const auto v = doc.array_push_back(node);
                doc.make_value(v, property_kind::uuid, as_bytes(ref.id));
            }
        }

        template <typename T>
        bool read_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.find_child(parent, name);

            if (node == data_node::Invalid || !doc.is_array(node))
            {
                return false;
            }

            for (u32 child = doc.child_next(node, data_node::Invalid); child != data_node::Invalid;
                child = doc.child_next(node, child))
            {
                const uuid id = doc.read_uuid(child).value_or({});
                array.emplace_back(id);
            }

            return true;
        }
    }

    expected<> save_model_json(const model& model, cstring_view destination)
    {
        data_document doc;
        doc.init();

        write_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        write_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return json::write(doc, destination);
    }

    expected<> load_model(model& model, cstring_view source)
    {
        data_document doc;

        if (auto e = json::read(doc, source); !e)
        {
            return e;
        }

        read_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        read_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return no_error;
    }

    expected<> save_skeleton_json(const skeleton& sk, cstring_view destination)
    {
        data_document doc;
        doc.init();

        const u32 joints = doc.child_array(doc.get_root(), "joints"_hsv, sk.jointsHierarchy.size32());

        u32 currentJointNode = data_node::Invalid;

        for (u32 i = 0; i < sk.jointsHierarchy.size32(); ++i)
        {
            const auto& joint = sk.jointsHierarchy[i];
            currentJointNode = doc.child_next(joints, currentJointNode);

            doc.make_object(currentJointNode);
            doc.child_value(currentJointNode, "name"_hsv, property_value_wrapper{string_view{joint.name}});
            doc.child_value(currentJointNode, "parentIndex"_hsv, property_value_wrapper{joint.parentIndex});

            write_child_array(doc, currentJointNode, "translation"_hsv, std::span{&joint.translation[0], 3});
            write_child_array(doc, currentJointNode, "rotation"_hsv, std::span{&joint.rotation[0], 3});
            write_child_array(doc, currentJointNode, "scale"_hsv, std::span{&joint.scale[0], 3});
        }

        return json::write(doc, destination);
    }

    expected<> load_skeleton(skeleton& sk, cstring_view source)
    {
        data_document doc;

        if (auto e = json::read(doc, source); !e)
        {
            return e;
        }

        const u32 joints = doc.find_child(doc.get_root(), "joints"_hsv);

        if (joints == data_node::Invalid || !doc.is_array(joints))
        {
            return "Invalid skeleton file"_err;
        }

        sk.jointsHierarchy.reserve(doc.children_count(joints));

        for (u32 child : doc.children(joints))
        {
            auto& joint = sk.jointsHierarchy.emplace_back();

            const expected name = doc.read_string(doc.find_child(child, "name"_hsv));
            const expected parentIndex = doc.read_u32(doc.find_child(child, "parentIndex"_hsv));

            const expected translation =
                read_child_array(doc, child, "translation"_hsv, std::span{&joint.translation[0], 3});
            const expected rotation = read_child_array(doc, child, "rotation"_hsv, std::span{&joint.rotation[0], 3});
            const expected scale = read_child_array(doc, child, "scale"_hsv, std::span{&joint.scale[0], 3});

            if (!name || !parentIndex || !translation || !rotation || !scale ||
                parentIndex.value() >= sk.jointsHierarchy.size())
            {
                return "Invalid data in skeleton"_err;
            }

            joint.name = string{name->str()};
            joint.parentIndex = parentIndex.value();
        }

        return no_error;
    }
}