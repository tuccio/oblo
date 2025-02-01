#include <oblo/project/project.hpp>

#include <oblo/properties/serialization/common.hpp>

namespace oblo
{
    namespace
    {
        void read_string(const data_document& doc, u32 node, string& s)
        {
            if (node != data_node::Invalid)
            {
                s = doc.read_string(node).value_or({}).str();
            }
        }
    }

    expected<project> project_load(cstring_view path)
    {
        project p;

        data_document doc;

        if (!json::read(doc, path))
        {
            return unspecified_error;
        }

        read_string(doc, doc.find_child(doc.get_root(), "name"_hsv), p.name);

        const auto directories = doc.find_child(doc.get_root(), "directories"_hsv);

        if (directories != data_node::Invalid && doc.is_object(directories))
        {
            read_string(doc, doc.find_child(directories, "assets"_hsv), p.assetsDir);
            read_string(doc, doc.find_child(directories, "artifacts"_hsv), p.artifactsDir);
            read_string(doc, doc.find_child(directories, "sources"_hsv), p.sourcesDir);
        }

        const auto modules = doc.find_child(doc.get_root(), "modules"_hsv);

        p.modules.clear();

        if (modules != data_node::Invalid && doc.is_array(modules))
        {
            for (auto child : doc.children(modules))
            {
                read_string(doc, child, p.modules.emplace_back());
            }
        }

        return p;
    }
}
