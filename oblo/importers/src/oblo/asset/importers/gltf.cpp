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
#include <oblo/graphics/components/animation_component.hpp>
#include <oblo/graphics/components/mesh_component.hpp>
#include <oblo/graphics/components/skin_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/resources/animation.hpp>
#include <oblo/scene/resources/animation_data.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/skeleton.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/serialization/animation_file.hpp>
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

        struct import_animation
        {
            u32 nodeIndex;
            u32 animationIndex;
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

        struct import_skin
        {
            u32 nodeIndex;
            u32 skeletonNodeIndex;
            bool skipped;
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

        enum class gltf_node_flag : u8
        {
            joint,
            enum_max,
        };

        data_format convert_component_type(int componentType, int type)
        {
            switch (type)
            {
            case TINYGLTF_TYPE_SCALAR:
                switch (componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_BYTE:
                    return data_format::i8;
                case TINYGLTF_COMPONENT_TYPE_SHORT:
                    return data_format::i16;
                case TINYGLTF_COMPONENT_TYPE_INT:
                    return data_format::i32;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    return data_format::u8;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return data_format::u16;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return data_format::u32;
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return data_format::f32;
                case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                    return data_format::f64;
                default:
                    return data_format::enum_max;
                }

            case TINYGLTF_TYPE_VEC2:
                switch (componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return data_format::vec2;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return data_format::vec2u16;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return data_format::vec2u;
                default:
                    return data_format::enum_max;
                }

            case TINYGLTF_TYPE_VEC3:
                switch (componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return data_format::vec3;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return data_format::vec3u16;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return data_format::vec3u;
                default:
                    return data_format::enum_max;
                }

            case TINYGLTF_TYPE_VEC4:
                switch (componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return data_format::vec4;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return data_format::vec4u16;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return data_format::vec4u;
                default:
                    return data_format::enum_max;
                }

            default:
                return data_format::enum_max;
            }
        }

        consteval string_view strip_namespace(string_view name)
        {
            const auto pos = name.find_last_of(':');

            if (pos == string_view::npos)
            {
                // Since it's consteval, we trap to fail compilation
                OBLO_DEBUGBREAK();
            }

            return {name.begin() + pos + 1, name.end()};
        }

        struct transform_trs
        {
            vec3 translation;
            quaternion rotation;
            vec3 scale;
        };

        transform_trs decompose_node_transform(const tinygltf::Node& node)
        {
            vec3 translationFallback{};
            quaternion rotationFallback{quaternion::identity()};
            vec3 scaleFallback{vec3::splat(1.f)};

            // GLTF can have either matrices or separate TRS fields
            // We account for both here
            if (node.matrix.size() == 16)
            {
                mat4 matrix;
                for (u32 i = 0; i < 4; ++i)
                {
                    for (u32 j = 0; j < 4; ++j)
                    {
                        matrix.columns[i][j] = f32(node.matrix[i * 4 + j]);
                    }
                }

                if (!decompose_matrix(matrix, translationFallback, rotationFallback, scaleFallback))
                {
                    log::error("Faled to decompose matrix for node {}", node.name);
                }
            }

            const vec3 translation = get_vec3_or(node.translation, translationFallback);
            const quaternion rotation = get_quaternion_or(node.rotation, rotationFallback);
            const vec3 scale = get_vec3_or(node.scale, scaleFallback);

            return {
                translation,
                rotation,
                scale,
            };
        }

        cstring_view make_or_get_joint_name(
            string_builder& jointNameBuilder, std::span<tinygltf::Node> nodes, i32 nodeIndex)
        {
            const tinygltf::Node& node = nodes[nodeIndex];

            if (node.name.empty())
            {
                return jointNameBuilder.clear().format("unnamed_joint_{}", nodeIndex).as<cstring_view>();
            }

            return {node.name.c_str(), node.name.length()};
        }
    }

    struct gltf::impl
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        dynamic_array<import_animation> importAnimations;
        dynamic_array<import_hierarchy> importHierarchies;
        dynamic_array<import_model> importModels;
        dynamic_array<import_mesh> importMeshes;
        dynamic_array<import_material> importMaterials;
        dynamic_array<import_image> importImages;
        dynamic_array<import_skeleton> importSkeletons;
        dynamic_array<import_skin> importSkins;

        dynamic_array<import_artifact> artifacts;
        dynamic_array<string> sourceFiles;
        string_builder sourceFileDir;
        uuid mainArtifactHint{};

        deque<embedded_image> embeddedImages;

        struct gltf_node_info
        {
            flags<gltf_node_flag> flags;
            resource_ref<animation> animation;
        };

        dynamic_array<gltf_node_info> gltfNodeFlags;

        void set_texture(material& m, hashed_string_view propertyName, int textureIndex) const
        {
            if (const auto imageIndex = find_image_from_texture(model, textureIndex);
                imageIndex >= 0 && usize(imageIndex) < importImages.size() && !importImages[imageIndex].id.is_nil())
            {
                m.set_property(propertyName, resource_ref<texture>(importImages[imageIndex].id));
            }
        }

        std::span<const byte> get_data_from_accessor(const tinygltf::Accessor& accessor) const
        {
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const usize byteOffset = accessor.byteOffset + bufferView.byteOffset;

            OBLO_ASSERT(bufferView.byteStride == 0);
            const data_format format = convert_component_type(accessor.componentType, accessor.type);
            const usize bytes = accessor.count * get_size_and_alignment(format).first;

            OBLO_ASSERT(bytes <= bufferView.byteLength);
            return {reinterpret_cast<const byte*>(buffer.data.data() + byteOffset), bytes};
        }

        void mark_skeleton(usize root)
        {
            gltfNodeFlags[root].flags |= gltf_node_flag::joint;

            for (const int child : model.nodes[root].children)
            {
                mark_skeleton(usize(child));
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

        m_impl->importAnimations.reserve(m_impl->model.animations.size());

        for (usize animationIndex = 0; animationIndex < m_impl->model.animations.size(); ++animationIndex)
        {
            const auto& gltfAnimation = m_impl->model.animations[animationIndex];

            nameBuilder.clear();

            if (gltfAnimation.name.empty())
            {
                nameBuilder.format("Animation#{}", animationIndex);
            }
            else
            {
                nameBuilder = gltfAnimation.name;
            }

            m_impl->importAnimations.push_back({
                .nodeIndex = u32(preview.nodes.size()),
                .animationIndex = u32(animationIndex),
            });

            preview.nodes.emplace_back(resource_type<animation>, nameBuilder.as<string>());
        }

        // Find all skeletons referenced by skins
        struct skeleton_node_info
        {
            bool isMarkedForImport{};
            u32 nodeIndex{};
        };

        dynamic_array<skeleton_node_info> skeletonNodeInfo;
        skeletonNodeInfo.resize(m_impl->model.nodes.size());

        m_impl->gltfNodeFlags.resize(m_impl->model.nodes.size());

        // Import all the skns, try to reuse skeleton nodes if possible, otherwise import them as new nodes.
        for (usize skinIndex = 0; skinIndex < m_impl->model.skins.size(); ++skinIndex)
        {
            const tinygltf::Skin& gltfSkin = m_impl->model.skins[skinIndex];

            auto& skinNode = m_impl->importSkins.emplace_back();

            if (gltfSkin.skeleton < 0)
            {
                log::debug("A skin does not specify the skeleton root, this is currently not supported");
                skinNode.skipped = true;
                continue;
            }

            const i32 gltfSkeletonIndex = gltfSkin.skeleton;
            nameBuilder = gltfSkin.name;

            if (nameBuilder.empty())
            {
                nameBuilder.format("Skin #{}", m_impl->importSkins.size() - 1);
            }

            skinNode.nodeIndex = preview.nodes.size32();
            preview.nodes.emplace_back(resource_type<skin>, nameBuilder.as<string>());

            auto& nodeInfo = skeletonNodeInfo[gltfSkeletonIndex];

            if (nodeInfo.isMarkedForImport)
            {
                skinNode.skeletonNodeIndex = nodeInfo.nodeIndex;
                continue;
            }

            nodeInfo.isMarkedForImport = true;
            nodeInfo.nodeIndex = preview.nodes.size32();

            m_impl->mark_skeleton(gltfSkeletonIndex);

            auto& skeletonNode = m_impl->importSkeletons.emplace_back();

            skeletonNode.sceneNodeRootIndex = gltfSkin.skeleton;
            skeletonNode.nodeIndex = nodeInfo.nodeIndex;
            skinNode.skeletonNodeIndex = nodeInfo.nodeIndex;

            nameBuilder = m_impl->model.nodes[gltfSkeletonIndex].name;

            if (nameBuilder.empty())
            {
                nameBuilder.format("Skeleton #{}", m_impl->importSkeletons.size() - 1);
            }

            preview.nodes.emplace_back(resource_type<skeleton>, nameBuilder.as<string>());
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

        // Parse config

        gltf_import_config cfg{};

        const auto& settings = ctx.get_settings();

        if (const auto generateMeshlets = settings.find_child(settings.get_root(), "generateMeshlets");
            generateMeshlets != data_node::Invalid)
        {
            cfg.generateMeshlets = settings.read_bool(generateMeshlets).value_or(true);
        }

        const std::span importNodeConfigs = ctx.get_import_node_configs();
        const std::span importNodes = ctx.get_import_nodes();

        // Associate image indices to the import uuids, since materials will need to refer to them, but import of
        // textures happens in parallel

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

        // Animations
        string_builder jointNameBuilder;

        for (const auto& importedAnimation : m_impl->importAnimations)
        {
            const auto& nodeConfig = importNodeConfigs[importedAnimation.nodeIndex];

            if (!nodeConfig.enabled)
            {
                continue;
            }

            const tinygltf::Animation& gltfAnim = m_impl->model.animations[importedAnimation.animationIndex];

            animation animArtifact;
            animArtifact.endianness = platform::endian::native;

            animArtifact.timeStart = std::numeric_limits<animation_time_t>::max();
            animArtifact.timeEnd = std::numeric_limits<animation_time_t>::lowest();

            // We could be smarter and merge together data refs that point to the same accessors, that might be
            // particuarly important for keyframes
            // For now we simply add the data as we encounter it

            bool anyError = false;

            for (const tinygltf::AnimationChannel& gltfChannel : gltfAnim.channels)
            {
                const int samplerIndex = gltfChannel.sampler;

                if (samplerIndex < 0 || usize(samplerIndex) >= gltfAnim.samplers.size())
                {
                    log::error("Invalid animation");
                    anyError = true;
                    break;
                }

                const tinygltf::AnimationSampler& sampler = gltfAnim.samplers[samplerIndex];

                const tinygltf::Accessor& dataAccessor = m_impl->model.accessors[sampler.output];

                const data_format format = convert_component_type(dataAccessor.componentType, dataAccessor.type);

                const tinygltf::Accessor& keyframesAccessor = m_impl->model.accessors[sampler.input];
                [[maybe_unused]] const data_format keyframeFormat =
                    convert_component_type(keyframesAccessor.componentType, keyframesAccessor.type);

                const std::span keyframesBytes = m_impl->get_data_from_accessor(keyframesAccessor);
                const usize keyframesCount = keyframesBytes.size_bytes() / sizeof(animation_time_t);

                const std::span keyframesData = {
                    start_lifetime_as_array<animation_time_t>(keyframesBytes.data(), keyframesCount),
                    keyframesCount,
                };

                const std::span dataBytes = m_impl->get_data_from_accessor(dataAccessor);

                string_view jointAnimationName;
                string_view componentPropertyName;
                uuid componentUuid;
                animation_data_kind dataKind;

                bool typeMismatch = false;

#define OBLO_GLTF_NAMEOF_PROPERTY(FullName) strip_namespace(OBLO_STRINGIZE(FullName));

                if (gltfChannel.target_path == "rotation")
                {
                    jointAnimationName = animation_data::properties::joint_rotation;
                    componentUuid = "7ef5fc6a-7b9c-491c-837f-d619747e9b50"_uuid;
                    componentPropertyName = OBLO_GLTF_NAMEOF_PROPERTY(position_component::value);
                    typeMismatch = typeMismatch || format != data_format::vec4;
                    dataKind = animation_data_kind::quaternion;
                }
                else if (gltfChannel.target_path == "translation")
                {
                    jointAnimationName = animation_data::properties::joint_translation;
                    componentUuid = "06d70f31-13c7-4c19-a1ca-19af48c5eb37"_uuid;
                    componentPropertyName = OBLO_GLTF_NAMEOF_PROPERTY(rotation_component::value);
                    typeMismatch = typeMismatch || format != data_format::vec3;
                    dataKind = animation_data_kind::any_vector;
                }
                else if (gltfChannel.target_path == "scale")
                {
                    jointAnimationName = animation_data::properties::joint_scale;
                    componentUuid = "3db97c8e-d984-494f-8644-026eb4bfa006"_uuid;
                    componentPropertyName = OBLO_GLTF_NAMEOF_PROPERTY(scale_component::value);
                    typeMismatch = typeMismatch || format != data_format::vec3;
                    dataKind = animation_data_kind::any_vector;
                }
                else
                {
                    // We don't handle weights (i.e. blend shapes) currently
                    log::error("Unknown property {} in animation {}",
                        gltfChannel.target_path,
                        importedAnimation.animationIndex);

                    anyError = true;
                    break;
                }

                if (typeMismatch || format == data_format::enum_max)
                {
                    log::error("Property {} in animation {} doesn't match the expected type",
                        gltfChannel.target_path,
                        importedAnimation.animationIndex);

                    anyError = true;
                    break;
                }

                impl::gltf_node_info& targetNodeInfo = m_impl->gltfNodeFlags[gltfChannel.target_node];
                const bool isJointAnimation = targetNodeInfo.flags.contains(gltf_node_flag::joint);

                if (!targetNodeInfo.animation)
                {
                    targetNodeInfo.animation = {.id = nodeConfig.id};
                }

                const usize dataSamplesCount = dataBytes.size_bytes() / get_size_and_alignment(format).first;

                if (keyframesCount != dataSamplesCount)
                {
                    log::error("Animation {} has a mismatch of samples and keyframes count ({} vs {})",
                        importedAnimation.animationIndex,
                        dataSamplesCount,
                        keyframesCount);

                    anyError = true;
                    break;
                }

                animation_channel& channel = animArtifact.channels.emplace_back();

                if (sampler.interpolation == "LINEAR")
                {
                    channel.interpolation = animation_interpolation::linear;
                }
                else if (sampler.interpolation == "STEP")
                {
                    channel.interpolation = animation_interpolation::cubic;

                    log::error("A cubic interpolation was found, but they are not fully supported yet");

                    anyError = true;
                    break;
                }
                else
                {
                    log::error("Unsupported interpolation mode in animation {}: {}",
                        importedAnimation.animationIndex,
                        sampler.interpolation);

                    anyError = true;
                    break;
                }

                channel.format = format;
                channel.dataKind = dataKind;

                if (!keyframesData.empty())
                {
                    animArtifact.timeStart = min(keyframesData.front(), animArtifact.timeStart);
                    animArtifact.timeEnd = max(keyframesData.back(), animArtifact.timeEnd);
                }

                animation_data::set_channel_keyframes(animArtifact, channel, keyframesData);

                if (isJointAnimation)
                {
                    const cstring_view jointName =
                        make_or_get_joint_name(jointNameBuilder, m_impl->model.nodes, gltfChannel.target_node);
                    channel.target = animation_target::joint;
                    animation_data::set_channel_joint_name(animArtifact, channel, jointName.c_str());
                    animation_data::set_channel_property_name(animArtifact, channel, jointAnimationName);
                }
                else
                {
                    channel.target = animation_target::component;
                    channel.componentUuid = componentUuid;
                    animation_data::set_channel_property_name(animArtifact, channel, componentPropertyName);
                }

                if (const expected e = animation_data::set_channel_data(animArtifact, channel, dataBytes, format); !e)
                {
                    log::error("Failed to parse animation data: {}", e.error().message);
                    anyError = true;
                    break;
                }
            }

            if (anyError)
            {
                continue;
            }

            string_builder outputPath;

            if (const expected e =
                    save_animation(animArtifact, ctx.get_output_path(nodeConfig.id, outputPath, ".oanimation"));
                !e)
            {
                log::error("Failed to save animation: {}", e.error().message);
                continue;
            }

            m_impl->artifacts.push_back({
                .id = nodeConfig.id,
                .type = resource_type<animation>,
                .name = importNodes[importedAnimation.nodeIndex].name,
                .path = outputPath.as<string>(),
            });
        }

        // Skeletons

        dynamic_array<skeleton::joint> jointsBuffer;
        jointsBuffer.reserve(256);

        for (const auto& importedSkeleton : m_impl->importSkeletons)
        {
            const auto& nodeConfig = importNodeConfigs[importedSkeleton.nodeIndex];

            if (!nodeConfig.enabled)
            {
                continue;
            }

            jointsBuffer.clear();

            const auto gatherSkeleton = [this, &jointsBuffer, &jointNameBuilder](auto&& recurse,
                                            i32 index,
                                            skeleton_joint_index_t parent) -> void
            {
                auto& current = m_impl->model.nodes[index];

                const skeleton_joint_index_t jointIndex = narrow_cast<skeleton_joint_index_t>(jointsBuffer.size());
                auto& joint = jointsBuffer.emplace_back();
                joint.parentIndex = parent;
                joint.name = make_or_get_joint_name(jointNameBuilder, m_impl->model.nodes, index).as<string>();

                const auto [translation, rotation, scale] = decompose_node_transform(current);

                joint.translation = translation;
                joint.rotation = rotation;
                joint.scale = scale;

                for (const i32 child : current.children)
                {
                    recurse(recurse, child, jointIndex);
                }
            };

            gatherSkeleton(gatherSkeleton, importedSkeleton.sceneNodeRootIndex, skeleton::joint::no_parent);

            skeleton skeletonArtifact;
            skeletonArtifact.jointsHierarchy.assign(jointsBuffer.begin(), jointsBuffer.end());

            string_builder outputPath;

            if (!save_skeleton_json(skeletonArtifact, ctx.get_output_path(nodeConfig.id, outputPath, ".oskeleton")))
            {
                log::error("Failed to save skeleton");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = nodeConfig.id,
                .type = resource_type<skeleton>,
                .name = importNodes[importedSkeleton.nodeIndex].name,
                .path = outputPath.as<string>(),
            });
        }

        for (u32 skinIndex = 0; skinIndex < m_impl->importSkins.size32(); ++skinIndex)
        {
            const auto& importedSkin = m_impl->importSkins[skinIndex];

            if (importedSkin.skipped)
            {
                continue;
            }

            const auto& modelNodeConfig = importNodeConfigs[importedSkin.nodeIndex];

            if (!modelNodeConfig.enabled)
            {
                continue;
            }

            const tinygltf::Skin& gltfSkin = m_impl->model.skins[skinIndex];

            const usize numJoints = gltfSkin.joints.size();

            skin skinArtifact;
            skinArtifact.invBindPoses.resize_default(numJoints);
            skinArtifact.jointNames.reserve(numJoints);

            if (gltfSkin.inverseBindMatrices < 0)
            {
                skinArtifact.invBindPoses.assign(numJoints, mat4::identity());
            }
            else
            {
                const auto& accessor = m_impl->model.accessors[gltfSkin.inverseBindMatrices];
                const auto& bufferView = m_impl->model.bufferViews[accessor.bufferView];
                const auto& buffer = m_impl->model.buffers[bufferView.buffer];
                const usize byteOffset = accessor.byteOffset + bufferView.byteOffset;

                const usize expectedSize = numJoints * sizeof(mat4);

                if (expectedSize != bufferView.byteLength)
                {
                    log::error("Failed to read inverse bind poses for skin '{}'", gltfSkin.name);
                    skinArtifact.invBindPoses.assign(numJoints, mat4::identity());
                }
                else
                {
                    std::memcpy(skinArtifact.invBindPoses.data(), buffer.data.data() + byteOffset, expectedSize);
                }
            }

            for (const i32 jointNodeIndex : gltfSkin.joints)
            {
                const cstring_view jointName =
                    make_or_get_joint_name(jointNameBuilder, m_impl->model.nodes, jointNodeIndex);
                skinArtifact.jointNames.emplace_back(jointName);
            }

            skinArtifact.skeleton = resource_ref<skeleton>{importNodeConfigs[importedSkin.skeletonNodeIndex].id};

            string_builder outputPath;
            if (!save_skin_json(skinArtifact, ctx.get_output_path(modelNodeConfig.id, outputPath, ".oskin")))
            {
                log::error("Failed to save skin");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = modelNodeConfig.id,
                .type = resource_type<skin>,
                .name = importNodes[importedSkin.nodeIndex].name,
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

                const impl::gltf_node_info& gltfNodeInfo = m_impl->gltfNodeFlags[nodeIndex];

                // Skip the skeleton, we don't need it in the entity hierarchy
                if (gltfNodeInfo.flags.contains(gltf_node_flag::joint))
                {
                    continue;
                }

                const tinygltf::Node& node = m_impl->model.nodes[nodeIndex];

                const auto [translation, rotation, scale] = decompose_node_transform(node);

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
                            const auto m = ecs_utility::create_named_physical_entity<mesh_component>(reg,
                                node.name.c_str(),
                                e,
                                vec3::splat(0.f),
                                quaternion::identity(),
                                vec3::splat(1.f));

                            const auto& importMesh = m_impl->importMeshes[meshIndex];
                            const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                            const auto& primitive =
                                m_impl->model.meshes[importMesh.meshIndex].primitives[importMesh.primitiveIndex];

                            auto& sm = reg.get<mesh_component>(m);
                            sm.mesh = resource_ref<mesh>{meshNodeConfig.id};
                            sm.material = resource_ref<material>{
                                primitive.material >= 0 ? m_impl->importMaterials[primitive.material].id : uuid{}};

                            if (node.skin >= 0)
                            {
                                const import_skin& importSkin = m_impl->importSkins[node.skin];

                                if (!importSkin.skipped)
                                {
                                    const auto& skinNode = importNodeConfigs[importSkin.nodeIndex];
                                    skin_component& skinComponent = reg.add<skin_component>(m);
                                    skinComponent.skin = resource_ref<skin>{skinNode.id};
                                }
                            }
                        }
                    }
                }

                if (gltfNodeInfo.animation)
                {
                    reg.add<animation_component>(e) = {.animation = gltfNodeInfo.animation};
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