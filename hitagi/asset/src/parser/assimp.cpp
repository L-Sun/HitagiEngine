#include <hitagi/asset/parser/assimp.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/gfx/utils.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>
#include <fmt/chrono.h>

#include <unordered_set>

using namespace hitagi::math;

namespace hitagi::asset {

inline constexpr auto get_matrix(const aiMatrix4x4& _mat) noexcept {
    return mat4f{
        {_mat.a1, _mat.a2, _mat.a3, _mat.a4},
        {_mat.b1, _mat.b2, _mat.b3, _mat.b4},
        {_mat.c1, _mat.c2, _mat.c3, _mat.c4},
        {_mat.d1, _mat.d2, _mat.d3, _mat.d4},
    };
};
inline constexpr auto get_vec3(const aiVector3D& v) noexcept {
    return vec3f{v.x, v.y, v.z};
};
inline constexpr auto get_color(const aiColor3D& c) noexcept {
    return vec3f{c.r, c.g, c.b};
}
inline constexpr auto get_color(const aiColor4D& c) noexcept {
    return vec4f{c.r, c.g, c.b, c.a};
};
inline constexpr auto get_primitive(unsigned int primitives) noexcept {
    if (primitives & aiPrimitiveType::aiPrimitiveType_LINE)
        return gfx::PrimitiveTopology::LineList;
    else if (primitives & aiPrimitiveType::aiPrimitiveType_POINT)
        return gfx::PrimitiveTopology::PointList;
    else if (primitives & aiPrimitiveType::aiPrimitiveType_TRIANGLE)
        return gfx::PrimitiveTopology::TriangleList;
    else {
        return gfx::PrimitiveTopology::Unkown;
    }
}

constexpr std::array<std::pair<std::string_view, aiTextureType>, 20> texture_key_map = {
    {
        {"diffuse", aiTextureType_DIFFUSE},
        {"specular", aiTextureType_SPECULAR},
        {"ambient", aiTextureType_AMBIENT},
        {"emissive", aiTextureType_EMISSIVE},
        {"height", aiTextureType_HEIGHT},
        {"normal", aiTextureType_NORMALS},
        {"shininess", aiTextureType_SHININESS},
        {"opacity", aiTextureType_OPACITY},
        {"dispacement", aiTextureType_DISPLACEMENT},
        {"lightmap", aiTextureType_LIGHTMAP},
        {"reflection", aiTextureType_REFLECTION},
        // PBR Material
        {"base_color", aiTextureType_BASE_COLOR},
        {"normal_camera", aiTextureType_NORMAL_CAMERA},
        {"emission_color", aiTextureType_EMISSION_COLOR},
        {"metalness", aiTextureType_METALNESS},
        {"diffuse_roughness", aiTextureType_DIFFUSE_ROUGHNESS},
        {"occlusion", aiTextureType_AMBIENT_OCCLUSION},
        // ---------
        {"sheen", aiTextureType_SHEEN},
        {"clearcoat", aiTextureType_CLEARCOAT},
        {"transmission", aiTextureType_TRANSMISSION},
    }};

constexpr std::array mat_color_keys = {
    "diffuse",
    "specular",
    "ambient",
    "emissive",
    "transparent",
    "reflective",
    // PBR
    "base",
    "sheen.factor",
};
constexpr std::array mat_float_keys = {
    "reflectivity",
    "opacity",
    "shininess",
    "shinpercent",
    "refracti",
    // PBR
    "metallicFactor",
};

auto AssimpParser::Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path) -> std::shared_ptr<Scene> {
    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    auto buffer = file_io_manager->SyncOpenAndReadBinary(path);

    if (buffer.Empty()) {
        logger->warn("Parsing a empty buffer");
        return nullptr;
    }

    core::Clock clock;

    Assimp::Importer importer;
    constexpr auto   flags =
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_GenSmoothNormals |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
        aiPostProcessSteps::aiProcess_LimitBoneWeights |
        aiPostProcessSteps::aiProcess_PopulateArmatureData;

    clock.Start();
    const aiScene* ai_scene = importer.ReadFileFromMemory(buffer.GetData(), buffer.GetDataSize(), flags, path.extension().string().c_str());
    if (!ai_scene) {
        logger->error("Can not parse the scene.");
        logger->error(importer.GetErrorString());
        return nullptr;
    }
    logger->trace("Parsing costs {:.3}.", clock.DeltaTime());
    clock.Tick();

    auto scene = std::make_shared<Scene>(ai_scene->mName.C_Str());

    // process camera
    std::pmr::unordered_map<std::string_view, std::shared_ptr<Camera>> camera_name_map;
    logger->trace("Parse cameras... Num: {}", ai_scene->mNumCameras);
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];

        auto camera = std::make_shared<Camera>(
            Camera::Parameters{
                .aspect         = _camera->mAspect,
                .near_clip      = _camera->mClipPlaneNear,
                .far_clip       = _camera->mClipPlaneFar,
                .horizontal_fov = _camera->mHorizontalFOV,
                .eye            = get_vec3(_camera->mPosition),
                .look_dir       = get_vec3(_camera->mLookAt),
                .up             = get_vec3(_camera->mUp),
            },
            _camera->mName.C_Str());

        auto&& [result, success] = camera_name_map.emplace(_camera->mName.C_Str(), camera);
        if (!success)
            logger->warn("A camera[{}] with the same name already exists!", _camera->mName.C_Str());
    }
    logger->trace("Parsing cameras costs {:.3}.", clock.DeltaTime());
    clock.Tick();

    // process light
    std::pmr::unordered_map<std::string_view, std::shared_ptr<Light>> light_name_map;
    logger->trace("Parse lights... Num: {}", ai_scene->mNumLights);
    for (size_t i = 0; i < ai_scene->mNumLights; i++) {
        const auto _light = ai_scene->mLights[i];

        Light::Parameters light_parameters;
        light_parameters.color = get_color(_light->mColorDiffuse);

        switch (_light->mType) {
            case aiLightSourceType::aiLightSource_AMBIENT:
                logger->warn("Unsupported light type: AMBIENT");
                break;
            case aiLightSourceType::aiLightSource_AREA:
                logger->warn("Unsupported light type: AREA");
                break;
            case aiLightSourceType::aiLightSource_DIRECTIONAL: {
                light_parameters.type      = Light::Type::Point;
                light_parameters.direction = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_POINT: {
                light_parameters.type     = Light::Type::Point;
                light_parameters.position = get_vec3(_light->mPosition);
            } break;
            case aiLightSourceType::aiLightSource_SPOT: {
                light_parameters.type             = Light::Type::Spot;
                light_parameters.inner_cone_angle = _light->mAngleInnerCone;
                light_parameters.outer_cone_angle = _light->mAngleOuterCone;
                light_parameters.position         = get_vec3(_light->mPosition);
                light_parameters.up               = normalize(get_vec3(_light->mUp));
                light_parameters.direction        = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_UNDEFINED:
                logger->warn("Unsupported light type: UNDEFINED");
                break;
            case aiLightSourceType::_aiLightSource_Force32Bit:
                logger->warn("Unsupported light type: Force32Bit");
                break;
            default:
                logger->warn("Unknown light type.");
                break;
        }
        auto light               = std::make_shared<Light>(light_parameters, _light->mName.C_Str());
        auto&& [result, success] = light_name_map.emplace(_light->mName.C_Str(), light);
        if (!success)
            logger->warn("A light[{}] with the same name already exists!", _light->mName.C_Str());
    }
    logger->trace("Parsing lights costs {}.", clock.DeltaTime());
    clock.Tick();

    // process textures
    logger->trace("Parse embedded texture... Num: {}", ai_scene->mNumTextures);
    std::pmr::unordered_map<const aiTexture*, std::shared_ptr<Texture>> textures;
    for (std::size_t i = 0; i < ai_scene->mNumTextures; i++) {
        auto                  _texture = ai_scene->mTextures[i];
        std::filesystem::path tex_path(_texture->mFilename.C_Str());

        std::shared_ptr<Texture> texture = nullptr;

        if (_texture->mHeight != 0) {
            texture = std::make_shared<Texture>(
                _texture->mWidth,
                _texture->mHeight,
                gfx::Format::B8G8R8A8_UNORM,
                core::Buffer(
                    _texture->mWidth * _texture->mHeight * gfx::get_format_byte_size(gfx::Format::B8G8R8A8_UNORM),
                    reinterpret_cast<const std::byte*>(_texture->pcData)),
                _texture->mFilename.C_Str());

            texture->SetPath(tex_path);
        } else {
            logger->trace("texture path: {}", _texture->mFilename.C_Str());
            if (_texture->CheckFormat("jpg")) {
                texture = std::make_shared<Texture>(tex_path);
            } else if (_texture->CheckFormat("png")) {
                texture = std::make_shared<Texture>(tex_path);
            } else if (_texture->CheckFormat("bmp")) {
                texture = std::make_shared<Texture>(tex_path);
            } else if (_texture->CheckFormat("tga")) {
                texture = std::make_shared<Texture>(tex_path);
            } else {
                logger->warn("Unsupported texture format: {}", _texture->achFormatHint);
            }
        }
        textures.emplace(_texture, texture);
    }
    logger->trace("Parsing texture costs {}.", clock.DeltaTime());
    clock.Tick();

    // process material
    logger->trace("Parse materials... Num: {}", ai_scene->mNumMaterials);
    std::pmr::vector<std::shared_ptr<MaterialInstance>> material_instances;
    for (std::size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        auto _material_instance = ai_scene->mMaterials[i];

        auto material_instance = std::make_shared<MaterialInstance>();

        if (aiString name; AI_SUCCESS == _material_instance->Get(AI_MATKEY_NAME, name))
            material_instance->SetName(name.C_Str());

        if (aiShadingMode shading_mode; _material_instance->Get(AI_MATKEY_SHADING_MODEL, shading_mode))
            logger->trace("Shading Mode: {}", magic_enum::enum_name(shading_mode));

        for (auto key : mat_color_keys) {
            if (aiColor3D _color; AI_SUCCESS == _material_instance->Get(fmt::format("$clr.{}", key).c_str(), 0, 0, _color)) {
                material_instance->SetParameter(key, get_color(_color));
            }
        }

        // set diffuse texture
        // TODO: blend mutiple texture
        for (const auto& [name, ai_key] : texture_key_map) {
            for (size_t i = 0; i < _material_instance->GetTextureCount(ai_key); i++) {
                aiString                 _path;
                std::shared_ptr<Texture> texture;

                if (AI_SUCCESS == _material_instance->GetTexture(ai_key, i, &_path)) {
                    if (auto _texture = ai_scene->GetEmbeddedTexture(_path.C_Str()); _texture) {
                        texture = textures.at(_texture);
                    } else {
                        texture = std::make_shared<Texture>(_path.C_Str());
                    }
                    material_instance->SetParameter(name, texture);
                }

                break;  // unsupported blend for now.
            }
        }

        material_instances.emplace_back(std::move(material_instance));
    }
    logger->trace("Parsing materials costs {:.3}.", clock.DeltaTime());
    clock.Tick();

    std::pmr::unordered_map<const aiMesh*, Mesh>        meshes;
    std::pmr::unordered_set<const aiNode*>              armature_nodes;
    std::pmr::unordered_map<const aiNode*, std::size_t> bone_nodes;  // bone and its index in the skeleton
    logger->trace("Parse meshes... Num: {}", ai_scene->mNumMeshes);
    for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
        auto ai_mesh = ai_scene->mMeshes[i];

        auto vertices = std::make_shared<VertexArray>(ai_mesh->mNumVertices, fmt::format("{}-vertices", ai_mesh->mName.C_Str()));

        // Read Position
        if (ai_mesh->HasPositions()) {
            vertices->Modify<VertexAttribute::Position>([&](auto positions) {
                std::memcpy(positions.data(), ai_mesh->mVertices, ai_mesh->mNumVertices * sizeof(aiVector3D));
            });
        }

        // Read Normal
        if (ai_mesh->HasNormals()) {
            vertices->Modify<VertexAttribute::Normal>([&](auto normals) {
                std::memcpy(normals.data(), ai_mesh->mNormals, ai_mesh->mNumVertices * sizeof(aiVector3D));
            });
        }

        // Read Color
        {
            if (ai_mesh->GetNumColorChannels() >= 4) {
                logger->warn("Only 4 color channels are supported now!");
            }
            constexpr std::array channels = {VertexAttribute::Color0, VertexAttribute::Color1, VertexAttribute::Color2, VertexAttribute::Color3};

            auto build_color = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                if (ai_mesh->HasVertexColors(I)) {
                    vertices->Modify<channels.at(I)>([&](auto colors) {
                        std::memcpy(colors.data(), ai_mesh->mColors[I], ai_mesh->mNumVertices * sizeof(aiVector3D));
                    });
                }
            };

            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (build_color(std::integral_constant<std::size_t, I>{}), ...);
            }(std::make_index_sequence<4>{});
        }

        // Read UV
        {
            if (ai_mesh->GetNumUVChannels() >= 4) {
                logger->warn("Only 4 uv channels are supported now!");
            }
            constexpr std::array channels = {VertexAttribute::UV0, VertexAttribute::UV1, VertexAttribute::UV2, VertexAttribute::UV3};

            auto build_uv = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                if (ai_mesh->HasTextureCoords(I)) {
                    vertices->Modify<channels.at(I)>([&](auto uvs) {
                        const auto _uvs = std::span<aiVector3D>(ai_mesh->mTextureCoords[I], ai_mesh->mNumVertices);
                        std::transform(_uvs.begin(), _uvs.end(), uvs.begin(), [](const aiVector3D& uv) { return vec2f(uv.x, uv.y); });
                    });
                }
            };
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (build_uv(std::integral_constant<std::size_t, I>{}), ...);
            }(std::make_index_sequence<4>{});
        }

        // Read Tangent and Bitangent
        if (ai_mesh->HasTangentsAndBitangents()) {
            vertices->Modify<VertexAttribute::Tangent>([&](auto tangents) {
                std::memcpy(tangents.data(), ai_mesh->mTangents, ai_mesh->mNumVertices * sizeof(aiVector3D));
            });

            vertices->Modify<VertexAttribute::Bitangent>([&](auto bi_tangents) {
                std::memcpy(bi_tangents.data(), ai_mesh->mBitangents, ai_mesh->mNumVertices * sizeof(aiVector3D));
            });
        }

        // Read bone blend parameters
        if (ai_mesh->HasBones()) {
            aiNode* armature_node = ai_mesh->mBones[0]->mArmature;
            armature_nodes.emplace(armature_node);

            for (std::size_t j = 0; j < ai_mesh->mNumBones; j++) {
                aiBone* _bone = ai_mesh->mBones[j];

                if (_bone->mArmature != armature_node) {
                    logger->warn("there are bones come from the same mesh, but related to different armature!");
                }

                // Calculate all bone index
                if (!bone_nodes.contains(_bone->mNode)) {
                    std::size_t bone_index_in_armature = 0;

                    std::function<void(aiNode*)> calculate_bone_index = [&](aiNode* node) -> void {
                        bone_nodes.emplace(node, bone_index_in_armature++);
                        for (auto child : std::span<aiNode*>(node->mChildren, node->mNumChildren))
                            calculate_bone_index(child);
                    };
                    for (auto skeleton : std::span<aiNode*>(armature_node->mChildren, armature_node->mNumChildren)) {
                        calculate_bone_index(skeleton);
                    }
                }
                std::size_t bone_index = bone_nodes.at(_bone->mNode);
                vertices->Modify<VertexAttribute::BlendIndex>([&](auto blend_indices) {
                    for (const aiVertexWeight& weight : std::span{_bone->mWeights, _bone->mNumWeights}) {
                        for (std::size_t k = 0; k < 4; j++) {
                            if (blend_indices[weight.mVertexId][k] == 0) {
                                blend_indices[weight.mVertexId][k] = bone_index;
                                break;
                            }
                        }
                    }
                });

                vertices->Modify<VertexAttribute::BlendWeight>([&](auto blend_weight) {
                    for (const aiVertexWeight& weight : std::span{_bone->mWeights, _bone->mNumWeights}) {
                        for (std::size_t k = 0; k < 4; j++) {
                            if (blend_weight[weight.mVertexId][k] == 0) {
                                blend_weight[weight.mVertexId][k] = weight.mWeight;
                                break;
                            }
                        }
                    }
                });
            }
        }

        std::size_t indices_count = 0;
        for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
            indices_count += ai_mesh->mFaces[face].mNumIndices;
        }
        auto indices = std::make_shared<IndexArray>(indices_count, IndexType::UINT32, fmt::format("{}-indices", ai_mesh->mName.C_Str()));

        // Read Indices
        indices->Modify<IndexType::UINT32>([&](auto values) {
            std::size_t p = 0;
            for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
                std::copy_n(ai_mesh->mFaces[face].mIndices, ai_mesh->mFaces[face].mNumIndices, values.data() + p);
                p += ai_mesh->mFaces[face].mNumIndices;
            }
        });

        Mesh mesh(vertices, indices, ai_mesh->mName.C_Str());

        mesh.sub_meshes.emplace_back(Mesh::SubMesh{
            .index_count       = indices_count,
            .index_offset      = 0,
            .vertex_offset     = 0,
            .material_instance = material_instances.at(ai_mesh->mMaterialIndex),
        });

        meshes.emplace(ai_mesh, std::move(mesh));
    };
    logger->trace("Parsing meshes costs {:.3}.", clock.DeltaTime());
    clock.Tick();

    auto create_mesh = [&](const aiNode* _node) -> std::shared_ptr<Mesh> {
        Mesh result;
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            result = result + meshes.at(ai_scene->mMeshes[_node->mMeshes[i]]);
        }
        return std::make_shared<Mesh>(result);
    };

    auto create_armature = [&](const aiNode* _node) -> std::shared_ptr<Armature> {
        auto armature = std::make_shared<Armature>();

        std::function<void(const aiNode*, const std::shared_ptr<Bone>&)> traversal = [&](const aiNode* _node, const std::shared_ptr<Bone>& parent) {
            auto bone       = std::make_shared<Bone>();
            bone->name      = _node->mName.C_Str();
            bone->transform = get_matrix(_node->mTransformation);
            bone->parent    = parent;
            parent->children.emplace_back(bone);

            armature->bone_collection.emplace_back(bone);

            for (auto child_bone : std::span<aiNode*>(_node->mChildren, _node->mNumChildren))
                traversal(child_bone, bone);
        };

        for (auto bone_node : std::span<aiNode*>(_node->mChildren, _node->mNumChildren)) {
            auto root_bone = std::make_shared<Bone>();
            armature->bone_collection.emplace_back(root_bone);
            traversal(bone_node, root_bone);
        }

        return armature;
    };

    std::function<std::shared_ptr<SceneNode>(const aiNode*)>
        convert = [&](const aiNode* _node) -> std::shared_ptr<SceneNode> {
        std::shared_ptr<SceneNode> node;

        std::string_view name      = _node->mName.C_Str();
        auto             transform = get_matrix(_node->mTransformation);

        // This node is a geometry
        if (_node->mNumMeshes > 0) {
            auto mesh      = create_mesh(_node);
            auto mesh_node = std::make_shared<MeshNode>(mesh, transform, name);
            scene->instance_nodes.emplace_back(mesh_node);
            node = mesh_node;
        }
        // This node is a camera
        else if (camera_name_map.contains(name)) {
            auto camera_node = std::make_shared<CameraNode>(camera_name_map.at(name), transform, name);
            scene->camera_nodes.emplace_back(camera_node);
            node = camera_node;
        }
        // This node is a light
        else if (light_name_map.contains(name)) {
            auto light_node = std::make_shared<LightNode>(light_name_map.at(name), transform, name);
            scene->light_nodes.emplace_back(light_node);
            node = light_node;
        }
        // This node is armature, it may contain multiple bone
        else if (armature_nodes.contains(_node)) {
            auto armature      = create_armature(_node);
            auto armature_node = std::make_shared<ArmatureNode>(armature, transform, name);
            scene->armature_nodes.emplace_back(armature_node);
            node = armature_node;
        }
        // This node is bone,
        else if (bone_nodes.contains(_node)) {
            // Skip bone node because it has processed in `create_armature()`
            return nullptr;
        }
        // This node is empty
        else {
            node = std::make_shared<SceneNode>(transform, name);
        }

        for (std::size_t i = 0; i < _node->mNumChildren; i++) {
            if (auto child = convert(_node->mChildren[i]); child)
                child->Attach(node);
        }

        return node;
    };
    scene->root = convert(ai_scene->mRootNode);
    if (scene->camera_nodes.empty()) {
        auto camera_node = scene->camera_nodes.emplace_back(std::make_shared<CameraNode>(std::make_shared<Camera>(Camera::Parameters{})));
        camera_node->Attach(scene->root);
    }
    scene->curr_camera = scene->camera_nodes.front();

    logger->trace("Parsing scene graph costs {:.3}.", clock.DeltaTime());
    clock.Tick();

    logger->info("Processing costs {:.3} totally.", clock.TotalTime());

    return scene;
}

}  // namespace hitagi::asset