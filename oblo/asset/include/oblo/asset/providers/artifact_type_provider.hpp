#pragma once

#include <oblo/asset/descriptors/artifact_type_descriptor.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo
{
    using artifact_type_provider = provider_service<artifact_type_descriptor>;
}