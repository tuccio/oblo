#pragma once

namespace oblo
{
    class abstract_syntax_tree;

    class script_api_provider
    {
    public:
        virtual ~script_api_provider() = default;

        virtual bool fetch_api(abstract_syntax_tree& tree) const = 0;
    };
}
