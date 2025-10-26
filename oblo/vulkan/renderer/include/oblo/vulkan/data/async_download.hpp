#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/thread/future.hpp>

namespace oblo::vk
{
    using async_download = future<dynamic_array<byte>>;
    using async_download_promise = promise<dynamic_array<byte>>;
}