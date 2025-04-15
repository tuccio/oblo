#include <oblo/smoke/framework.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo::smoke
{
    template <typename T>
    resource_ptr<T> find_first_resource_from_asset(
        const resource_registry& resourceRegistry, const asset_registry& assetRegistry, uuid assetId)
    {
        buffered_array<uuid, 16> artifacts;

        if (!assetRegistry.find_asset_artifacts(assetId, artifacts))
        {
            return {};
        }

        for (auto uuid : artifacts)
        {
            artifact_meta meta;

            if (assetRegistry.find_artifact_by_id(uuid, meta) && meta.type == resource_type<T>)
            {
                return resourceRegistry.get_resource(uuid).as<T>();
            }
        }

        return {};
    }

    inline test_task wait_for_asset_processing(const test_context& ctx, const asset_registry& assetRegistry)
    {
        do
        {
            co_await ctx.next_frame();
        } while (assetRegistry.get_running_import_count() > 0);

        co_return;
    }
}
