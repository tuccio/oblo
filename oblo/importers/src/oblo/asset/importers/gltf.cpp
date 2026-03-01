#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/processing/mesh_processing.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/skeleton.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/serialization/model_file.hpp>
#include <oblo/scene/serialization/skeleton_file.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/thread/parallel_for.hpp>

#include <tinygltf/implementation.hpp>

namespace oblo::importers
{
    namespace
    {
        struct embedded_image
        {
            u32 imageIndex;
            string sourceFile;
        };

        struct import_hierarchy
        {
            u32 nodeIndex;
            u32 sceneIndex;
        };

        struct import_model
        {
            u32 meshIndex;
            u32 nodeIndex;
            u32 primitiveBegin;
        };

        struct import_mesh
        {
            u32 meshIndex;
            u32 primitiveIndex;
            u32 nodeIndex;
            string_builder outputPath;
            bool wasImported;
        };

        struct import_material
        {
            u32 nodeIndex;
            uuid id;
        };

        struct import_image
        {
            const embedded_image* embeddedImage{};
            usize subImportIndex;
            uuid id;
        };

        struct import_skeleton
        {
            u32 nodeIndex;
            i32 sceneNodeRootIndex;
        };

        int find_image_from_texture(const tinygltf::Model& model, int textureIndex)
        {
            if (textureIndex < 0)
            {
                return textureIndex;
            }

            return usize(textureIndex) < model.textures.size() ? model.textures[textureIndex].source : -1;
        }

        struct gltf_import_config
        {
            bool generateMeshlets{true};
        };

        vec3 get_vec3_or(const std::vector<double>& value, vec3 fallback)
        {
            if (value.size() >= 3)
            {
                return {f32(value[0]), f32(value[1]), f32(value[2])};
            }
            else
            {
                return fallback;
            }
        }

        quaternion get_quaternion_or(const std::vector<double>& value, quaternion fallback)
        {
            if (value.size() == 4)
            {
                return {
                    f32(value[0]),
                    f32(value[1]),
                    f32(value[2]),
                    f32(value[3]),
                };
            }
            else
            {
                return fallback;
            }
        }
    }

    struct gltf::impl
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        dynamic_array<import_hierarchy> importHierarchies;
        dynamic_array<import_model> importModels;
        dynamic_array<import_mesh> importMeshes;
        dynamic_array<import_material> importMaterials;
        dynamic_array<import_image> importImages;
        dynamic_array<import_skeleton> importSkeletons;

        dynamic_array<import_artifact> artifacts;
        dynamic_array<string> sourceFiles;
        string_builder sourceFileDir;
        uuid mainArtifactHint{};

        deque<embedded_image> embeddedImages;

        void set_texture(material& m, hashed_string_view propertyName, int textureIndex) const
        {
            if (const auto imageIndex = find_image_from_texture(model, textureIndex);
                imageIndex >= 0 && usize(imageIndex) < importImages.size() && !importImages[imageIndex].id.is_nil())
            {
                m.set_property(propertyName, resource_ref<texture>(importImages[imageIndex].id));
            }
        }
    };

    gltf::gltf() = default;

    gltf::~gltf() = default;

    bool gltf::init(const import_config& config, import_preview& preview)
    {
        m_impl = allocate_unique<impl>();

        // Seems like TinyGLTF wants std::string
        const auto sourceFileStr = config.sourceFile.as<std::string>();

        m_impl->sourceFiles.reserve(1 + m_impl->model.buffers.size() + m_impl->model.images.size());
        m_impl->sourceFiles.emplace_back(config.sourceFile);

        filesystem::parent_path(m_impl->sourceFiles[0], m_impl->sourceFileDir);

        struct image_load_args
        {
            string_builder pathBuilder;
            deque<embedded_image>& embeddedImages;
            cstring_view workDir;
        };

        image_load_args imageLoadArgs{
            .embeddedImages = m_impl->embeddedImages,
            .workDir = config.workDir,
        };

        // The current approach is to write these images to disk, so we can use the regular sub-import infrastructure.
        m_impl->loader.SetImageLoader(
            [](tinygltf::Image* image,
                int imageIndex,
                std::string*,
                std::string*,
                int,
                int,
                const unsigned char* data,
                int bytes,
                void* userdata)
            {
                image_load_args& args = *static_cast<image_load_args*>(userdata);

                args.pathBuilder.clear().append(args.workDir);

                if (image->name.empty())
                {
                    args.pathBuilder.append_path_separator().format("Image{}", imageIndex);
                }
                else
                {
                    args.pathBuilder.append_path(image->name);
                }

                const string_view mime = image->mimeType;

                if (auto slashIdx = mime.find_first_of('/'); slashIdx != string_view::npos)
                {
                    args.pathBuilder.append(".").append(image->mimeType.substr(slashIdx + 1));
                }

                args.embeddedImages.push_back({
                    .imageIndex = u32(imageIndex),
                    .sourceFile = args.pathBuilder.as<string>(),
                });

                return filesystem::write_file(args.pathBuilder,
                    as_bytes(std::span{data, usize(bytes)}),
                    filesystem::write_mode::binary)
                    .has_value();
            },
            &imageLoadArgs);

        bool success;

        std::string errors;
        std::string warnings;

        if (sourceFileStr.ends_with(".glb"))
        {
            success = m_impl->loader.LoadBinaryFromFile(&m_impl->model, &errors, &warnings, sourceFileStr);
        }
        else if (sourceFileStr.ends_with(".gltf"))
        {
            success = m_impl->loader.LoadASCIIFromFile(&m_impl->model, &errors, &warnings, sourceFileStr);
        }
        else
        {
            // TODO: Report error
            return false;
        }

        if (!success)
        {
            log::error("Import of {} failed:\n{}", sourceFileStr, errors);
            return false;
        }

        string_builder nameBuilder;
        string_builder primitiveNameBuilder;

        for (u32 meshIndex = 0; meshIndex < m_impl->model.meshes.size(); ++meshIndex)
        {
            const auto& gltfMesh = m_impl->model.meshes[meshIndex];

            nameBuilder.clear();

            if (gltfMesh.name.empty())
            {
                nameBuilder.format("Model#{}", meshIndex);
            }
            else
            {
                nameBuilder = gltfMesh.name;
            }

            m_impl->importModels.push_back({
                .meshIndex = meshIndex,
                .nodeIndex = u32(preview.nodes.size()),
                .primitiveBegin = u32(m_impl->importMeshes.size()),
            });

            preview.nodes.emplace_back(resource_type<model>, nameBuilder.as<string>());

            for (u32 primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex)
            {
                primitiveNameBuilder = nameBuilder;
                primitiveNameBuilder.format("/Mesh#{}", primitiveIndex);

                m_impl->importMeshes.push_back({
                    .meshIndex = meshIndex,
                    .primitiveIndex = primitiveIndex,
                    .nodeIndex = u32(preview.nodes.size()),
                    .wasImported = false,
                });

                preview.nodes.emplace_back(resource_type<mesh>, primitiveNameBuilder.as<string>());
            }
        }

        // Find all skeletons referenced by skins
        dynamic_array<i32> allSkeletons;

        for (const tinygltf::Skin& skin : m_impl->model.skins)
        {
            if (skin.skeleton < 0)
            {
                continue;
            }

            allSkeletons.push_back(skin.skeleton);
        }

        // Remove duplicates
        std::sort(allSkeletons.begin(), allSkeletons.end());
        allSkeletons.erase(std::unique(allSkeletons.begin(), allSkeletons.end()), allSkeletons.end());

        m_impl->importSkeletons.reserve(allSkeletons.size());

        for (const i32 nodeIdx : allSkeletons)
        {
            auto& skeletonNode = m_impl->importSkeletons.emplace_back();

            skeletonNode.sceneNodeRootIndex = nodeIdx;

            string_view nodeName{m_impl->model.nodes[nodeIdx].name};

            if (nodeName.empty())
            {
                nodeName = "Unnamed skeleton";
            }

            skeletonNode.nodeIndex = preview.nodes.size32();
            preview.nodes.emplace_back(resource_type<skeleton>, string{nodeName});
        }

        m_impl->importImages.resize(m_impl->model.images.size());

        for (auto& embeddedImage : m_impl->embeddedImages)
        {
            m_impl->importImages[embeddedImage.imageIndex].embeddedImage = &embeddedImage;
        }

        std::string stdStringBuf;

        for (u32 imageIndex = 0; imageIndex < m_impl->model.images.size(); ++imageIndex)
        {
            auto& gltfImage = m_impl->model.images[imageIndex];
            auto& importImage = m_impl->importImages[imageIndex];

            if (importImage.embeddedImage)
            {
                if (gltfImage.name.empty())
                {
                    nameBuilder.clear().format("Image#{}", imageIndex);
                }
                else
                {
                    nameBuilder = gltfImage.name;
                }
            }
            else
            {
                if (gltfImage.uri.empty())
                {
                    log::warn(
                        "A texture was skipped because URI is not set, maybe it's embedded in the GLTF but this is "
                        "not supported currently.");

                    continue;
                }

                if (!tinygltf::URIDecode(gltfImage.uri, &stdStringBuf, nullptr))
                {
                    log::error("Failed to decode URI {}", gltfImage.uri);
                    continue;
                }

                nameBuilder = stdStringBuf;
            }

            importImage.subImportIndex = preview.children.size();
            auto& subImport = preview.children.emplace_back();

            preview.nodes.emplace_back(resource_type<texture>, nameBuilder.as<string>());

            if (importImage.embeddedImage)
            {
                nameBuilder = importImage.embeddedImage->sourceFile;
            }
            else
            {
                nameBuilder.clear().append(m_impl->sourceFileDir).append_path(stdStringBuf.c_str());
            }

            subImport.sourceFile = nameBuilder.as<string>();
            subImport.skipSourceFiles = importImage.embeddedImage;
        }

        m_impl->importMaterials.reserve(m_impl->model.materials.size());

        for (u32 materialIndex = 0; materialIndex < m_impl->model.materials.size(); ++materialIndex)
        {
            auto& gltfMaterial = m_impl->model.materials[materialIndex];
            m_impl->importMaterials.emplace_back(preview.nodes.size32());
            preview.nodes.emplace_back(resource_type<material>, string{gltfMaterial.name.c_str()});

            const auto metallicRoughness = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;

            if (metallicRoughness >= 0 && metallicRoughness == gltfMaterial.occlusionTexture.index &&
                usize(metallicRoughness) < m_impl->importImages.size())
            {
                auto& importImage = m_impl->importImages[metallicRoughness];

                auto& subImport = preview.children[importImage.subImportIndex];
                auto& settings = subImport.settings;

                // When the texture is an ORM map, we drop the occlusion and swap roughness and metalness back
                settings.init();

                const u32 swizzle = settings.child_array(settings.get_root(), "swizzle");

                settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{2}));
                settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{1}));
            }
        }

        m_impl->importHierarchies.reserve(m_impl->model.scenes.size());

        for (u32 sceneIndex = 0; sceneIndex < m_impl->model.scenes.size(); ++sceneIndex)
        {
            auto& gltfScene = m_impl->model.scenes[sceneIndex];

            m_impl->importHierarchies.push_back({
                .nodeIndex = preview.nodes.size32(),
                .sceneIndex = sceneIndex,
            });

            if (gltfScene.name.empty())
            {
                nameBuilder.clear().format("Scene#{}", sceneIndex);
            }
            else
            {
                nameBuilder = gltfScene.name;
            }

            preview.nodes.emplace_back(resource_type<entity_hierarchy>, nameBuilder.as<string>());
        }

        return true;
    }

    bool gltf::import(import_context ctx)
    {
        entity_hierarchy_serialization_context ehCtx;

        if (!ehCtx.init())
        {
            log::error("Failed to initialize entity hierarchy context");
            return false;
        }

        gltf_import_config cfg{};

        const auto& settings = ctx.get_settings();

        if (const auto generateMeshlets = settings.find_child(settings.get_root(), "generateMeshlets");
            generateMeshlets != data_node::Invalid)
        {
            cfg.generateMeshlets = settings.read_bool(generateMeshlets).value_or(true);
        }

        const std::span importNodeConfigs = ctx.get_import_node_configs();
        const std::span importNodes = ctx.get_import_nodes();

        for (usize i = 0; i < m_impl->importImages.size(); ++i)
        {
            const std::span childNodes = ctx.get_child_import_nodes(i);
            const std::span childNodeConfigs = ctx.get_child_import_node_configs(i);

            if (childNodes.size() == 1 && childNodeConfigs[0].enabled &&
                childNodes[0].artifactType == resource_type<texture>)
            {
                auto& image = m_impl->importImages[i];
                image.id = childNodeConfigs[0].id;
            }
        }

        for (auto& material : m_impl->importMaterials)
        {
            const auto& nodeConfig = importNodeConfigs[material.nodeIndex];

            if (!nodeConfig.enabled)
            {
                continue;
            }

            oblo::material materialArtifact;

            const auto materialIndex = &material - m_impl->importMaterials.data();
            auto& gltfMaterial = m_impl->model.materials[materialIndex];

            auto& pbr = gltfMaterial.pbrMetallicRoughness;

            m_impl->set_texture(materialArtifact, pbr::AlbedoTexture, pbr.baseColorTexture.index);
            m_impl->set_texture(materialArtifact, pbr::MetalnessRoughnessTexture, pbr.metallicRoughnessTexture.index);
            m_impl->set_texture(materialArtifact, pbr::NormalMapTexture, gltfMaterial.normalTexture.index);
            m_impl->set_texture(materialArtifact, pbr::EmissiveTexture, gltfMaterial.emissiveTexture.index);

            materialArtifact.set_property<material_type_tag::linear_color>(pbr::Albedo,
                get_vec3_or(pbr.baseColorFactor, vec3::splat(1.f)));

            materialArtifact.set_property(pbr::Metalness, f32(pbr.metallicFactor));
            materialArtifact.set_property(pbr::Roughness, f32(pbr.roughnessFactor));

            {
                auto emissiveFactor = get_vec3_or(gltfMaterial.emissiveFactor, vec3::splat(0.f));
                const auto [r, g, b] = emissiveFactor;
                const auto highest = max(r, g, b);

                f32 emissiveMultiplier = 1.f;

                if (highest > 1.f)
                {
                    emissiveFactor = emissiveFactor / highest;
                    emissiveMultiplier = highest;
                }

                materialArtifact.set_property<material_type_tag::linear_color>(pbr::Emissive, emissiveFactor);
                materialArtifact.set_property(pbr::EmissiveMultiplier, emissiveMultiplier);
            }

            f32 ior{1.5f};

            if (const auto it = gltfMaterial.extensions.find("KHR_materials_ior");
                it != gltfMaterial.extensions.end() && it->second.IsObject())
            {
                ior = f32(it->second.Get("ior").GetNumberAsDouble());
            }

            materialArtifact.set_property(pbr::IndexOfRefraction, ior);

            string name = importNodes[material.nodeIndex].name;

            string_builder buffer;

            if (name.empty())
            {
                buffer.clear().format("Material#{}", materialIndex);
                name = buffer.as<string>();
            }

            if (const auto path = ctx.get_output_path(nodeConfig.id, buffer, ".omaterial");
                !materialArtifact.save(path))
            {
                log::error("Failed to save material to {}", path);
                continue;
            }

            m_impl->artifacts.push_back({
                .id = nodeConfig.id,
                .type = resource_type<oblo::material>,
                .name = std::move(name),
                .path = buffer.as<string>(),
            });

            material.id = nodeConfig.id;
        }

        const u32 numThreads = job_manager::get()->get_num_threads();
        const usize numBuffers = m_impl->model.buffers.size();

        // Allocate buffers that are written by all threads
        dynamic_array<bool> usedBuffers;
        usedBuffers.resize(numThreads * numBuffers);

        parallel_for(
            [this, &importNodeConfigs, &cfg, &ctx, &usedBuffers, numBuffers](job_range range)
            {
                buffered_array<mesh_attribute, 16> attributes;
                buffered_array<gltf_accessor, 32> sources;

                const usize offset = job_manager::get()->get_current_thread() * numBuffers;

                const std::span usedBuffersSpan = std::span{usedBuffers}.subspan(offset, numBuffers);

                for (u32 meshIndex = range.begin; meshIndex < range.end; ++meshIndex)
                {
                    attributes.clear();

                    auto& importMesh = m_impl->importMeshes[meshIndex];
                    const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                    if (!meshNodeConfig.enabled)
                    {
                        continue;
                    }

                    const auto& primitive =
                        m_impl->model.meshes[importMesh.meshIndex].primitives[importMesh.primitiveIndex];

                    mesh srcMesh;

                    if (!load_mesh(srcMesh,
                            m_impl->model,
                            primitive,
                            attributes,
                            sources,
                            &usedBuffersSpan,
                            mesh_post_process::generate_tanget_space))
                    {
                        log::error("Failed to parse mesh");
                        continue;
                    }

                    mesh outMesh;

                    if (cfg.generateMeshlets)
                    {
                        if (!mesh_processing::build_meshlets(srcMesh, outMesh))
                        {
                            log::error("Failed to build meshlets");
                            continue;
                        }
                    }
                    else
                    {
                        outMesh = std::move(srcMesh);
                    }

                    outMesh.update_aabb();

                    const cstring_view outputPath =
                        ctx.get_output_path(meshNodeConfig.id, importMesh.outputPath, ".gltf");

                    if (!save_mesh(outMesh, outputPath))
                    {
                        log::error("Failed to save mesh");
                        continue;
                    }

                    importMesh.wasImported = true;
                }
            },
            job_range{0u, m_impl->importMeshes.size32()},
            1u);

        // Merge the results from different threads together
        for (u32 i = 1; i < numThreads; ++i)
        {
            const usize offset = i * numBuffers;

            const std::span usedBuffersSpan = std::span{usedBuffers}.subspan(offset, numBuffers);

            for (u32 j = 0; j < numBuffers; ++j)
            {
                usedBuffers[j] |= usedBuffersSpan[j];
            }
        }

        usedBuffers.resize(numBuffers);

        dynamic_array<skeleton::joint> jointsBuffer;
        jointsBuffer.reserve(256);

        for (const auto& importedSkeleton : m_impl->importSkeletons)
        {
            const auto& modelNodeConfig = importNodeConfigs[importedSkeleton.nodeIndex];

            if (!modelNodeConfig.enabled)
            {
                continue;
            }

            jointsBuffer.clear();

            const auto gatherSkeleton = [this, &jointsBuffer](auto&& recurse, i32 index, u32 parent) -> void
            {
                auto& current = m_impl->model.nodes[index];

                const u32 jointIndex = jointsBuffer.size32();
                auto& joint = jointsBuffer.emplace_back();
                joint.parentIndex = parent;
                joint.name = string{current.name};

                joint.translation = get_vec3_or(current.translation, vec3::splat(0.f));
                joint.rotation = get_quaternion_or(current.rotation, quaternion::identity());
                joint.scale = get_vec3_or(current.scale, vec3::splat(1.f));

                for (const i32 child : current.children)
                {
                    recurse(recurse, child, jointIndex);
                }
            };

            gatherSkeleton(gatherSkeleton, importedSkeleton.sceneNodeRootIndex, skeleton::joint::no_parent);

            skeleton skeletonArtifact;
            skeletonArtifact.jointsHierarchy.assign(jointsBuffer.begin(), jointsBuffer.end());

            string_builder outputPath;

            if (!save_skeleton_json(skeletonArtifact,
                    ctx.get_output_path(modelNodeConfig.id, outputPath, ".oskeleton")))
            {
                log::error("Failed to save skeleton");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = modelNodeConfig.id,
                .type = resource_type<skeleton>,
                .name = importNodes[importedSkeleton.nodeIndex].name,
                .path = outputPath.as<string>(),
            });
        }

        for (const auto& model : m_impl->importModels)
        {
            const auto& modelNodeConfig = importNodeConfigs[model.nodeIndex];

            if (!modelNodeConfig.enabled)
            {
                continue;
            }

            const auto& gltfMesh = m_impl->model.meshes[model.meshIndex];

            oblo::model modelAsset;

            const auto numPrimitives = gltfMesh.primitives.size();
            modelAsset.meshes.reserve(numPrimitives);
            modelAsset.materials.reserve(numPrimitives);

            for (u32 meshIndex = model.primitiveBegin; meshIndex < model.primitiveBegin + numPrimitives; ++meshIndex)
            {
                const auto& importMesh = m_impl->importMeshes[meshIndex];
                const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                if (!importMesh.wasImported)
                {
                    continue;
                }

                const auto& primitive =
                    m_impl->model.meshes[importMesh.meshIndex].primitives[importMesh.primitiveIndex];

                modelAsset.meshes.emplace_back(meshNodeConfig.id);
                modelAsset.materials.emplace_back(
                    primitive.material >= 0 ? m_impl->importMaterials[primitive.material].id : uuid{});

                m_impl->artifacts.push_back({
                    .id = meshNodeConfig.id,
                    .type = resource_type<mesh>,
                    .name = importNodes[importMesh.nodeIndex].name,
                    .path = importMesh.outputPath.as<string>(),
                });
            }

            string_builder outputPath;

            if (!save_model_json(modelAsset, ctx.get_output_path(modelNodeConfig.id, outputPath, ".omodel")))
            {
                log::error("Failed to save mesh");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = modelNodeConfig.id,
                .type = resource_type<oblo::model>,
                .name = importNodes[model.nodeIndex].name,
                .path = outputPath.as<string>(),
            });

            if (m_impl->importModels.size() == 1)
            {
                m_impl->mainArtifactHint = modelNodeConfig.id;
            }
        }

        for (const auto& hierarchy : m_impl->importHierarchies)
        {
            const auto& hierarchyNodeConfig = importNodeConfigs[hierarchy.nodeIndex];

            if (!hierarchyNodeConfig.enabled)
            {
                continue;
            }

            entity_hierarchy h;

            if (!h.init(ehCtx.get_type_registry()))
            {
                log::error("Failed to initialize entity hierarchy");
                continue;
            }

            auto& reg = h.get_entity_registry();

            struct stack_info
            {
                ecs::entity parent;
                i32 nodeIndex;
            };

            deque<stack_info> stack;

            const auto& gltfScene = m_impl->model.scenes[hierarchy.sceneIndex];

            for (auto node : gltfScene.nodes)
            {
                stack.push_back({.nodeIndex = node});
            }

            while (!stack.empty())
            {
                const auto [parent, nodeIndex] = stack.back();
                stack.pop_back();

                auto& node = m_impl->model.nodes[nodeIndex];

                const vec3 translation = get_vec3_or(node.translation, vec3::splat(0.f));
                const quaternion rotation = get_quaternion_or(node.rotation, quaternion::identity());
                const vec3 scale = get_vec3_or(node.scale, vec3::splat(1.f));

                const auto e = ecs_utility::create_named_physical_entity(reg,
                    node.name.c_str(),
                    parent,
                    translation,
                    rotation,
                    scale);

                if (node.mesh >= 0)
                {
                    const usize modelIndex = usize(node.mesh);

                    if (modelIndex < m_impl->importModels.size())
                    {
                        auto& model = m_impl->importModels[modelIndex];
                        auto& gltfMesh = m_impl->model.meshes[model.meshIndex];

                        const auto numPrimitives = gltfMesh.primitives.size();

                        for (u32 meshIndex = model.primitiveBegin; meshIndex < model.primitiveBegin + numPrimitives;
                            ++meshIndex)
                        {
                            const auto m = ecs_utility::create_named_physical_entity<static_mesh_component>(reg,
                                node.name.c_str(),
                                e,
                                vec3::splat(0.f),
                                quaternion::identity(),
                                vec3::splat(1.f));

                            const auto& importMesh = m_impl->importMeshes[meshIndex];
                            const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                            const auto& primitive =
                                m_impl->model.meshes[importMesh.meshIndex].primitives[importMesh.primitiveIndex];

                            auto& sm = reg.get<static_mesh_component>(m);
                            sm.mesh = resource_ref<mesh>{meshNodeConfig.id};
                            sm.material = resource_ref<material>{
                                primitive.material >= 0 ? m_impl->importMaterials[primitive.material].id : uuid{}};
                        }
                    }
                }

                for (auto child : node.children)
                {
                    stack.push_back({.parent = e, .nodeIndex = child});
                }
            }

            string_builder outputPath;
            ctx.get_output_path(hierarchyNodeConfig.id, outputPath, ".ohierarchy");

            if (!h.save(outputPath, ehCtx))
            {
                log::error("Failed to save entity hierarchy");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = hierarchyNodeConfig.id,
                .type = resource_type<oblo::entity_hierarchy>,
                .name = importNodes[hierarchy.nodeIndex].name,
                .path = outputPath.as<string>(),
            });

            if (m_impl->importHierarchies.size() == 1)
            {
                m_impl->mainArtifactHint = hierarchyNodeConfig.id;
            }
        }

        string_builder bufferPathBuilder;

        for (usize i = 0; i < usedBuffers.size(); ++i)
        {
            if (usedBuffers[i])
            {
                auto& buffer = m_impl->model.buffers[i];

                if (buffer.uri.empty() || buffer.uri.starts_with("data:"))
                {
                    continue;
                }

                bufferPathBuilder.clear().append(m_impl->sourceFileDir).append_path(buffer.uri);

                if (filesystem::exists(bufferPathBuilder).value_or(false))
                {
                    m_impl->sourceFiles.emplace_back(bufferPathBuilder.as<string>());
                }
            }
        }

        return true;
    }

    file_import_results gltf::get_results()
    {
        return {
            .artifacts = m_impl->artifacts,
            .sourceFiles = m_impl->sourceFiles,
            .mainArtifactHint = m_impl->mainArtifactHint,
        };
    }
}