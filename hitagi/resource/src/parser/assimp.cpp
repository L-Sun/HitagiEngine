#include <hitagi/parser/assimp.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/asset_manager.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <unordered_set>
#include <stack>

using namespace hitagi::math;

namespace hitagi::resource {

mat4f get_matrix(const aiMatrix4x4& _mat) {
    return {
        {_mat.a1, _mat.a2, _mat.a3, _mat.a4},
        {_mat.b1, _mat.b2, _mat.b3, _mat.b4},
        {_mat.c1, _mat.c2, _mat.c3, _mat.c4},
        {_mat.d1, _mat.d2, _mat.d3, _mat.d4},
    };
};
vec3f get_vec3(const aiVector3D& v) {
    return {v.x, v.y, v.z};
};
vec3f get_color(const aiColor3D& c) {
    return {c.r, c.g, c.b};
}
vec4f get_color(const aiColor4D& c) {
    return {c.r, c.g, c.b, c.a};
};

PrimitiveType get_primitive(unsigned int primitives) {
    if (primitives & aiPrimitiveType::aiPrimitiveType_LINE)
        return PrimitiveType::LineList;
    else if (primitives & aiPrimitiveType::aiPrimitiveType_POINT)
        return PrimitiveType::PointList;
    else if (primitives & aiPrimitiveType::aiPrimitiveType_TRIANGLE)
        return PrimitiveType::TriangleList;
    else {
        return PrimitiveType::Unkown;
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

AssimpParser::AssimpParser(std::filesystem::path ext) : m_Hint(std::move(ext)) {}

std::optional<Scene> AssimpParser::Parse(const core::Buffer& buffer, const std::filesystem::path& root_path) {
    auto logger = spdlog::get("AssetManager");

    if (buffer.Empty()) {
        logger->warn("Parsing a empty buffer");
        return std::nullopt;
    }

    core::Clock clock;

    Assimp::Importer importer;
    auto             flag =
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_GenSmoothNormals |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
        aiPostProcessSteps::aiProcess_LimitBoneWeights |
        aiPostProcessSteps::aiProcess_PopulateArmatureData;

    clock.Start();
    const aiScene* ai_scene = importer.ReadFileFromMemory(buffer.GetData(), buffer.GetDataSize(), flag, m_Hint.string().c_str());
    if (!ai_scene) {
        logger->error("Can not parse the scene.");
        logger->error(importer.GetErrorString());
        return std::nullopt;
    }
    logger->info("Parsing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    Scene scene;
    scene.name = ai_scene->mName.C_Str();

    // process camera
    std::pmr::unordered_map<std::string_view, std::shared_ptr<Camera>> camera_name_map;
    logger->debug("Parse cameras... Num: {}", ai_scene->mNumCameras);
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];

        auto camera       = std::make_shared<Camera>();
        camera->aspect    = _camera->mAspect;
        camera->near_clip = _camera->mClipPlaneNear;
        camera->far_clip  = _camera->mClipPlaneFar;
        camera->fov       = _camera->mHorizontalFOV;
        camera->eye       = get_vec3(_camera->mPosition);
        camera->look_dir  = get_vec3(_camera->mLookAt);
        camera->up        = get_vec3(_camera->mUp);

        auto&& [result, success] = camera_name_map.emplace(_camera->mName.C_Str(), camera);
        if (!success)
            logger->warn("A camera[{}] with the same name already exists!", _camera->mName.C_Str());
    }
    logger->info("Parsing cameras costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    // process light
    std::pmr::unordered_map<std::string_view, std::shared_ptr<Light>> light_name_map;
    logger->debug("Parse lights... Num: {}", ai_scene->mNumLights);
    for (size_t i = 0; i < ai_scene->mNumLights; i++) {
        const auto _light = ai_scene->mLights[i];

        auto light   = std::make_shared<Light>();
        light->color = get_color(_light->mColorDiffuse);

        switch (_light->mType) {
            case aiLightSourceType::aiLightSource_AMBIENT:
                std::cerr << "[AssimpParser] " << std::endl;
                logger->warn("Unsupport light type: AMBIEN");
                break;
            case aiLightSourceType::aiLightSource_AREA:
                logger->warn("Unsupport light type: AREA");
                break;
            case aiLightSourceType::aiLightSource_DIRECTIONAL: {
                light->type      = Light::Type::Point;
                light->direction = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_POINT: {
                light->type     = Light::Type::Point;
                light->position = get_vec3(_light->mPosition);
            } break;
            case aiLightSourceType::aiLightSource_SPOT: {
                light->type             = Light::Type::Spot;
                light->inner_cone_angle = _light->mAngleInnerCone;
                light->outer_cone_angle = _light->mAngleOuterCone;
                light->position         = get_vec3(_light->mPosition);
                light->up               = normalize(get_vec3(_light->mUp));
                light->direction        = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_UNDEFINED:
                logger->warn("Unsupport light type: UNDEFINED");
                break;
            case aiLightSourceType::_aiLightSource_Force32Bit:
                logger->warn("Unsupport light type: Force32Bit");
                break;
            default:
                logger->warn("Unknown light type.");
                break;
        }
        auto&& [result, success] = light_name_map.emplace(_light->mName.C_Str(), light);
        if (!success)
            logger->warn("A light[{}] with the same name already exists!", _light->mName.C_Str());
    }
    logger->info("Parsing lights costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    // process textures
    logger->debug("Parse embedded texture... Num: {}", ai_scene->mNumTextures);
    std::pmr::unordered_map<const aiTexture*, std::shared_ptr<Texture>> textures;
    for (std::size_t i = 0; i < ai_scene->mNumTextures; i++) {
        auto                     _texture = ai_scene->mTextures[i];
        std::shared_ptr<Texture> texture  = nullptr;

        if (_texture->mHeight != 0) {
            texture             = std::make_shared<Texture>();
            texture->width      = _texture->mWidth;
            texture->height     = _texture->mHeight;
            texture->format     = Format::R8G8B8A8_UINT;
            texture->cpu_buffer = core::Buffer(texture->width * texture->height * 4);
            auto dest           = texture->cpu_buffer.Span<Vector<std::uint8_t, 4>>();
            auto src            = std::span<aiTexel>(_texture->pcData, _texture->mWidth * _texture->mHeight);
            std::transform(src.begin(), src.end(), dest.begin(), [](const aiTexel& color) {
                return Vector<std::uint8_t, 4>{color.r, color.g, color.b, color.a};
            });
        } else {
            logger->debug("texture path: {}", _texture->mFilename.C_Str());
            auto buffer = core::Buffer(_texture->mWidth, reinterpret_cast<const std::byte*>(_texture->pcData));
            if (_texture->CheckFormat("jpg")) {
                texture = asset_manager->ImportTexture(buffer, ImageFormat::JPEG);
            } else if (_texture->CheckFormat("png")) {
                texture = asset_manager->ImportTexture(buffer, ImageFormat::PNG);
            } else if (_texture->CheckFormat("bmp")) {
                texture = asset_manager->ImportTexture(buffer, ImageFormat::BMP);
            } else if (_texture->CheckFormat("tga")) {
                texture = asset_manager->ImportTexture(buffer, ImageFormat::TGA);
            } else {
                logger->warn("Unsupport texture format: {}", _texture->achFormatHint);
            }
        }

        textures.emplace(_texture, texture);
    }
    logger->info("Parsing texture costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    // process material
    logger->debug("Parse materials... Num: {}", ai_scene->mNumMaterials);
    std::pmr::vector<std::shared_ptr<MaterialInstance>> material_instances;
    for (std::size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        auto _material_instance = ai_scene->mMaterials[i];

        std::shared_ptr<Material> material = nullptr;

        // Use Phong
        if (aiColor3D color; AI_SUCCESS == _material_instance->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
            material = asset_manager->GetMaterial("Phong");
        }
        // Use PBR
        if (aiColor3D color; AI_SUCCESS == _material_instance->Get(AI_MATKEY_BASE_COLOR, color)) {
            material = asset_manager->GetMaterial("PBR");
        }

        if (material == nullptr) {
            logger->error("Missing builtin material! Please check the folder: assets/materials");
            break;
        }

        std::shared_ptr<MaterialInstance> material_instance;

        if (aiString name; AI_SUCCESS == _material_instance->Get(AI_MATKEY_NAME, name))
            material_instance = material->CreateInstance(name.C_Str());

        if (aiShadingMode shading_mode; _material_instance->Get(AI_MATKEY_SHADING_MODEL, shading_mode))
            logger->debug("Shading Mode: {}", magic_enum::enum_name(shading_mode));

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
                        auto texture  = std::make_shared<Texture>();
                        texture->path = _path.C_Str();
                    }
                    material_instance->SetParameter(name, texture);
                }

                break;  // unsupport blend for now.
            }
        }

        material_instances.emplace_back(std::move(material_instance));
    }
    logger->info("Parsing materials costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    std::pmr::unordered_map<const aiMesh*, Mesh>        meshes;
    std::pmr::unordered_set<const aiNode*>              armature_nodes;
    std::pmr::unordered_map<const aiNode*, std::size_t> bone_nodes;  // bone and its index in the skeleton
    logger->debug("Parse meshes... Num: {}", ai_scene->mNumMeshes);
    for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
        auto ai_mesh = ai_scene->mMeshes[i];
        // Read Indices
        std::size_t num_indices = 0;
        for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
            num_indices += ai_mesh->mFaces[face].mNumIndices;
        }

        Mesh mesh{
            .vertices = std::make_shared<VertexArray>(ai_mesh->mNumVertices, ai_mesh->mName.C_Str()),
            .indices  = std::make_shared<IndexArray>(num_indices, ai_mesh->mName.C_Str()),
        };

        // Read Position
        if (ai_mesh->HasPositions()) {
            mesh.vertices->Modify<VertexAttribute::Position>([&](auto positions) {
                const auto _positions = std::span<aiVector3D>(ai_mesh->mVertices, ai_mesh->mNumVertices);
                std::transform(_positions.begin(), _positions.end(), positions.begin(), [](const auto& v) { return get_vec3(v); });
            });
        }

        // Read Normal
        if (ai_mesh->HasNormals()) {
            mesh.vertices->Modify<VertexAttribute::Normal>([&](auto normals) {
                const auto _normals = std::span<aiVector3D>(ai_mesh->mNormals, ai_mesh->mNumVertices);
                std::transform(_normals.begin(), _normals.end(), normals.begin(), [](const auto& v) { return get_vec3(v); });
            });
        }

        // Read Color
        {
            if (ai_mesh->GetNumColorChannels() >= 4) {
                logger->warn("Only 4 color channels are supported now!");
            }
            constexpr std::array channels    = {VertexAttribute::Color0, VertexAttribute::Color1, VertexAttribute::Color2, VertexAttribute::Color3};
            auto                 build_color = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                if (ai_mesh->HasVertexColors(I)) {
                    mesh.vertices->Modify<channels.at(I)>([&](auto colors) {
                        const auto _colors = std::span<aiColor4D>(ai_mesh->mColors[I], ai_mesh->mNumVertices);
                        std::transform(_colors.begin(), _colors.end(), colors.begin(), [](const auto& c) { return get_color(c); });
                    });
                }
            };
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (build_color(std::integral_constant<std::size_t, I>{}), ...);
            }
            (std::make_index_sequence<4>{});
        }

        // Read UV
        {
            if (ai_mesh->GetNumUVChannels() >= 4) {
                logger->warn("Only 4 uv channels are supported now!");
            }
            constexpr std::array channels    = {VertexAttribute::UV0, VertexAttribute::UV1, VertexAttribute::UV2, VertexAttribute::UV3};
            auto                 build_color = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                if (ai_mesh->HasTextureCoords(I)) {
                    mesh.vertices->Modify<channels.at(I)>([&](auto uvs) {
                        const auto _uvs = std::span<aiVector3D>(ai_mesh->mTextureCoords[I], ai_mesh->mNumVertices);
                        std::transform(_uvs.begin(), _uvs.end(), uvs.begin(), [](const aiVector3D& uv) { return vec2f(uv.x, uv.y); });
                    });
                }
            };
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (build_color(std::integral_constant<std::size_t, I>{}), ...);
            }
            (std::make_index_sequence<4>{});
        }

        // Read Tangent and Bitangent
        if (ai_mesh->HasTangentsAndBitangents()) {
            mesh.vertices->Modify<VertexAttribute::Tangent>([&](auto tangents) {
                const auto _tangents = std::span<aiVector3D>(ai_mesh->mTangents, ai_mesh->mNumVertices);
                std::transform(_tangents.begin(), _tangents.end(), tangents.begin(), [](const auto& v) { return get_vec3(v); });
            });

            mesh.vertices->Modify<VertexAttribute::Bitangent>([&](auto bi_tangents) {
                const auto _bi_tangents = std::span<aiVector3D>(ai_mesh->mBitangents, ai_mesh->mBitangents);
                std::transform(_bi_tangents.begin(), _bi_tangents.end(), bi_tangents.begin(), [](const auto& v) { return get_vec3(v); });
            });
        }

        // Read bone blend parameters
        if (ai_mesh->HasBones()) {
            mesh.vertices->Modify<VertexAttribute::BlendIndex, VertexAttribute::BlendWeight>([&](auto blend_indices, auto blend_weight) {
                aiNode* armature_node = ai_mesh->mBones[0]->mArmature;
                armature_nodes.emplace(armature_node);

                for (std::size_t j = 0; j < ai_mesh->mNumBones; j++) {
                    aiBone* _bone = ai_mesh->mBones[j];

                    if (_bone->mArmature != armature_node) {
                        logger->warn("there are bones come from the same mesh, but releated to diffrent armature!");
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

                    for (auto weight : std::span<aiVertexWeight>(_bone->mWeights, _bone->mNumWeights)) {
                        for (std::size_t j = 0; j < 4; j++) {
                            if (blend_indices[weight.mVertexId][j] == 0) {
                                blend_indices[weight.mVertexId][j] = bone_index;
                                blend_weight[weight.mVertexId][j]  = weight.mWeight;
                                break;
                            }
                        }
                    }
                }
            });
        }

        // Read Indices
        mesh.indices->Modify<IndexType::UINT32>([&](auto array) {
            std::size_t p = 0;
            for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
                std::copy_n(ai_mesh->mFaces[face].mIndices, ai_mesh->mFaces[face].mNumIndices, array.data() + p);
                p += ai_mesh->mFaces[face].mNumIndices;
            }
        });

        mesh.sub_meshes.emplace_back(Mesh::SubMesh{
            .index_count       = mesh.indices->index_count,
            .index_offset      = 0,
            .vertex_offset     = 0,
            .primitive         = get_primitive(ai_mesh->mPrimitiveTypes),
            .material_instance = material_instances.at(ai_mesh->mMaterialIndex),
        });

        meshes.emplace(ai_mesh, std::move(mesh));
    };
    logger->info("Parsing meshes costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    auto create_mesh = [&](const aiNode* _node) -> std::shared_ptr<Mesh> {
        std::pmr::vector<Mesh> sub_meshes;
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            sub_meshes.emplace_back(meshes.at(ai_scene->mMeshes[_node->mMeshes[i]]));
        }
        return std::make_shared<Mesh>(merge_meshes(sub_meshes));
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

        std::string_view name = _node->mName.C_Str();

        // This node is a geometry
        if (_node->mNumMeshes > 0) {
            auto mesh_node    = std::make_shared<MeshNode>();
            mesh_node->object = create_mesh(_node);
            scene.instance_nodes.emplace_back(mesh_node);
            node = mesh_node;
        }
        // This node is a camera
        else if (camera_name_map.contains(name)) {
            auto camera_node    = std::make_shared<CameraNode>();
            camera_node->object = camera_name_map.at(name);
            scene.camera_nodes.emplace_back(camera_node);
            node = camera_node;
        }
        // This node is a light
        else if (light_name_map.contains(name)) {
            auto light_node    = std::make_shared<LightNode>();
            light_node->object = light_name_map.at(name);
            scene.light_nodes.emplace_back(light_node);
            node = light_node;
        }
        // This node is armature, it may contain multiple bone
        else if (armature_nodes.contains(_node)) {
            auto armature_node    = std::make_shared<ArmatureNode>();
            armature_node->object = create_armature(_node);
            scene.armature_nodes.emplace_back(armature_node);
            node = armature_node;
        }
        // This node is bone,
        else if (bone_nodes.contains(_node)) {
            // Skip bone node because it has processed in `create_armature()`
            return nullptr;
        }
        // This node is empty
        else {
            node = std::make_shared<SceneNode>();
        }

        node->name      = name;
        node->transform = get_matrix(_node->mTransformation);

        for (std::size_t i = 0; i < _node->mNumChildren; i++) {
            if (auto child = convert(_node->mChildren[i]); child)
                child->Attach(node);
        }

        return node;
    };
    scene.root = convert(ai_scene->mRootNode);
    if (scene.camera_nodes.empty()) {
        auto camera_node = scene.camera_nodes.emplace_back(std::make_shared<CameraNode>("Default Camera"));
        camera_node->Attach(scene.root);
    }
    scene.curr_camera = scene.camera_nodes.front();

    logger->info("Parsing scnene graph costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    logger->info("Processing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.TotalTime()).count());

    return scene;
}

}  // namespace hitagi::resource