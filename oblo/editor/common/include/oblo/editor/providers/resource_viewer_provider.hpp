#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/services/resource_viewer.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    using resource_viewer_create_fn = unique_ptr<resource_viewer> (*)();

    struct resource_viewer_descriptor
    {
        uuid resourceType{};
        resource_viewer_create_fn createViewer{};
    };

    using resource_viewer_provider = provider_service<resource_viewer_descriptor>;
}