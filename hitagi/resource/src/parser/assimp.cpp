#include <hitagi/parser/assimp.hpp>
#include <hitagi/core/timer.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <unordered_set>

using namespace hitagi::math;

namespace hitagi::resource {

mat4f get_matrix(const aiMatrix4x4& _mat) {
    mat4f ret(reinterpret_cast<const ai_real*>(&_mat));
    return transpose(ret);
};
vec3f get_vec3(const aiVector3D& v) {
    return {v.x, v.y, v.z};
};
vec3f get_vec3(const aiColor3D& v) {
    return {v.r, v.g, v.b};
};
vec4f get_vec4(const aiColor4D& c) {
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

void AssimpParser::Parse(const core::Buffer& buffer, Scene& scene, std::pmr::vector<std::shared_ptr<Material>>& materials) {
    auto logger = spdlog::get("AssetManager");

    if (buffer.Empty()) {
        logger->warn("[Assimp] Parsing a empty buffer");
        return;
    }

    core::Clock clock;

    Assimp::Importer importer;
    auto             flag =
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_GenSmoothNormals |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices;

    clock.Start();
    const aiScene* ai_scene = importer.ReadFileFromMemory(buffer.GetData(), buffer.GetDataSize(), flag);
    if (!ai_scene) {
        logger->error("[Assimp] Can not parse the scene.");
        return;
    }
    logger->info("[Assimp] Parsing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    scene.name = ai_scene->mName.C_Str();

    // process camera
    std::unordered_map<std::string_view, Camera> camera_name_map;
    logger->debug("Parse cameras... Num: {}", ai_scene->mNumCameras);
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];

        Camera camera;
        camera.aspect    = _camera->mAspect;
        camera.near_clip = _camera->mClipPlaneNear;
        camera.far_clip  = _camera->mClipPlaneFar;
        camera.fov       = _camera->mHorizontalFOV;
        camera.eye       = get_vec3(_camera->mPosition);
        camera.look_dir  = get_vec3(_camera->mUp);
        camera.up        = get_vec3(_camera->mLookAt);
        camera.Update();

        auto&& [result, success] = camera_name_map.emplace(_camera->mName.C_Str(), camera);
        if (!success)
            logger->warn("A camera[{}] with the same name already exists!", _camera->mName.C_Str());
    }
    logger->info("[Assimp] Parsing cameras costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    // process light
    std::pmr::unordered_map<std::string_view, Light> light_name_map;
    logger->debug("Parse cameras... Num: {}", ai_scene->mNumLights);
    for (size_t i = 0; i < ai_scene->mNumLights; i++) {
        const auto _light = ai_scene->mLights[i];

        Light light;
        light.color = get_vec3(_light->mColorDiffuse);

        switch (_light->mType) {
            case aiLightSourceType::aiLightSource_AMBIENT:
                std::cerr << "[AssimpParser] " << std::endl;
                logger->warn("[Assimp] Unsupport light type: AMBIEN");
                break;
            case aiLightSourceType::aiLightSource_AREA:
                logger->warn("[Assimp] Unsupport light type: AREA");
                break;
            case aiLightSourceType::aiLightSource_DIRECTIONAL: {
                light.type      = Light::Type::Point;
                light.direction = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_POINT: {
                light.type     = Light::Type::Point;
                light.position = get_vec3(_light->mPosition);
            } break;
            case aiLightSourceType::aiLightSource_SPOT: {
                light.type             = Light::Type::Spot;
                light.inner_cone_angle = _light->mAngleInnerCone;
                light.outer_cone_angle = _light->mAngleOuterCone;
                light.position         = get_vec3(_light->mPosition);
                light.up               = normalize(get_vec3(_light->mUp));
                light.direction        = normalize(get_vec3(_light->mDirection));
            } break;
            case aiLightSourceType::aiLightSource_UNDEFINED:
                logger->warn("[Assimp] Unsupport light type: UNDEFINED");
                break;
            case aiLightSourceType::_aiLightSource_Force32Bit:
                logger->warn("[Assimp] Unsupport light type: Force32Bit");
                break;
            default:
                logger->warn("[Assimp] Unknown light type.");
                break;
        }
        auto&& [result, success] = light_name_map.emplace(_light->mName.C_Str(), light);
        if (!success)
            logger->warn("A light[{}] with the same name already exists!", _light->mName.C_Str());
    }
    logger->info("[Assimp] Parsing lights costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    // process material
    logger->debug("Parse materials... Num: {}", ai_scene->mNumMaterials);
    std::pmr::vector<MaterialInstance> material_instances;
    for (std::size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        auto              _material = ai_scene->mMaterials[i];
        Material::Builder builder;

        // set material diffuse color
        if (aiColor3D ambientColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor))
            builder.AppendParameterInfo("ambient", vec4f(ambientColor.r, ambientColor.g, ambientColor.b, 1.0f));
        if (aiColor3D diffuseColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
            builder.AppendParameterInfo("diffuse", vec4f(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f));
        // set material specular color
        if (aiColor3D specularColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor))
            builder.AppendParameterInfo("specular", vec4f(specularColor.r, specularColor.g, specularColor.b, 1.0f));

        // set material emission color
        if (aiColor3D emissiveColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor))
            builder.AppendParameterInfo("emission", vec4f(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f));
        // set material transparent color
        if (aiColor3D transparentColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_TRANSPARENT, transparentColor))
            builder.AppendParameterInfo("transparency", vec4f(transparentColor.r, transparentColor.g, transparentColor.b, 1.0f));
        // set material shiness
        if (float shininess = 0; AI_SUCCESS == _material->Get(AI_MATKEY_SHININESS, shininess))
            builder.AppendParameterInfo("specular_power", shininess);
        // set material opacity
        if (float opacity = 0; AI_SUCCESS == _material->Get(AI_MATKEY_OPACITY, opacity))
            builder.AppendParameterInfo("opacity", opacity);

        // set diffuse texture
        // TODO: blend mutiple texture
        const std::pmr::unordered_map<std::pmr::string, aiTextureType> map = {
            {
                {"diffuse", aiTextureType::aiTextureType_DIFFUSE},
                {"specular", aiTextureType::aiTextureType_SPECULAR},
                {"emission", aiTextureType::aiTextureType_EMISSIVE},
                {"opacity", aiTextureType::aiTextureType_OPACITY},
                // {"transparency", },
                {"normal", aiTextureType::aiTextureType_NORMALS},
            }};
        for (auto&& [key1, key2] : map) {
            for (size_t i = 0; i < _material->GetTextureCount(key2); i++) {
                aiString _path;
                if (AI_SUCCESS == _material->GetTexture(aiTextureType::aiTextureType_DIFFUSE, i, &_path)) {
                    builder.AppendTextureName(key1, _path.C_Str());
                }
                break;  // unsupport blend for now.
            }
        }

        // TODO adapte assimp shader
        builder
            .SetVertexShader("assets/shaders/color.hlsl")
            .SetPixelShader("assets/shaders/color.hlsl");

        auto material = builder.Build();
        auto instance = material->CreateInstance();

        auto iter = std::find_if(materials.begin(),
                                 materials.end(),
                                 [material](auto m) -> bool {
                                     return *m == *material;
                                 });

        // A new material type
        if (iter == materials.end()) {
            materials.emplace_back(material);
        }
        // A exists material type
        else {
            instance.SetMaterial(*iter);
        }

        material_instances.emplace_back(std::move(instance));
    };
    logger->info("[Assimp] Parsing materials costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    std::pmr::unordered_map<aiMesh*, Mesh> meshes;
    logger->debug("Parse meshes... Num: {}", ai_scene->mNumMeshes);
    for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
        auto ai_mesh = ai_scene->mMeshes[i];
        // Read Indices
        std::size_t num_indices = 0;
        for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
            num_indices += ai_mesh->mFaces[face].mNumIndices;
        }

        Mesh mesh{
            .vertices = std::make_shared<VertexArray>(ai_mesh->mNumVertices),
            .indices  = std::make_shared<IndexArray>(num_indices, IndexType::UINT32),
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
                        std::transform(_colors.begin(), _colors.end(), colors.begin(), [](const auto& c) { return get_vec4(c); });
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

        // Read Indices
        mesh.indices->Modify<IndexType::UINT32>([&](auto array) {
            std::size_t p = 0;
            for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++) {
                std::copy_n(ai_mesh->mFaces[face].mIndices, ai_mesh->mFaces[face].mNumIndices, array.data() + p);
                p += ai_mesh->mFaces[face].mNumIndices;
            }
        });

        // // bone
        // if (ai_mesh->HasBones()) {
        //     for (size_t bone_index = 0; bone_index < ai_mesh->mNumBones; bone_index++) {
        //         auto _bone = ai_mesh->mBones[bone_index];
        //         auto bone  = mesh.CreateNewBone(_bone->mName.C_Str());
        //         bone_name_map.emplace(bone->GetName(), bone);

        //         bone->SetName(_bone->mName.C_Str());
        //         for (size_t i = 0; i < _bone->mNumWeights; i++)
        //             bone->SetWeight(_bone->mWeights[i].mVertexId, _bone->mWeights[i].mWeight);
        //         bone->SetBindTransformMatrix(get_matrix(_bone->mOffsetMatrix));
        //     }
        // }
        mesh.vertices->name = ai_mesh->mName.C_Str();
        mesh.indices->name  = ai_mesh->mName.C_Str();
        mesh.sub_meshes.emplace_back(Mesh::SubMesh{
            .index_count   = mesh.indices->index_count,
            .vertex_offset = 0,
            .index_offset  = 0,
            .material      = material_instances.at(ai_mesh->mMaterialIndex),
        });

        if (auto material = material_instances[ai_mesh->mMaterialIndex].GetMaterial().lock(); material) {
            if (material->primitive == PrimitiveType::Unkown) {
                material->primitive = get_primitive(ai_mesh->mPrimitiveTypes);
            }
        }

        meshes.emplace(ai_mesh, std::move(mesh));
    };
    logger->info("[Assimp] Parsing meshes costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    auto create_mesh = [&](const aiNode* _node) -> Mesh {
        std::pmr::vector<Mesh> sub_meshes;
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            sub_meshes.emplace_back(meshes.at(ai_scene->mMeshes[_node->mMeshes[i]]));
        }
        return merge_meshes(sub_meshes);
    };

    std::function<void(const aiNode*, ecs::Entity parent)>
        convert = [&](const aiNode* _node, ecs::Entity parent) -> void {
        std::pmr::string name = _node->mName.C_Str();

        ecs::Entity node;
        Hierarchy   hierarchy_componet{.parentID = parent};
        Transform   transform_component{get_matrix(_node->mTransformation)};

        // The node is a geometry
        if (_node->mNumMeshes > 0) {
            node = scene.geometries.emplace_back(scene.world.CreateEntity(create_mesh(_node), hierarchy_componet, transform_component));
        }
        // The node is a camera
        else if (camera_name_map.count(name) != 0) {
            node = scene.cameras.emplace_back(scene.world.CreateEntity(camera_name_map.at(name), hierarchy_componet, transform_component));
        }
        // The node is a light
        else if (light_name_map.count(name) != 0) {
            node = scene.lights.emplace_back(scene.world.CreateEntity(light_name_map.at(name), hierarchy_componet, transform_component));
        }
        // // The node is bone
        // else if (bone_name_map.count(name) != 0) {
        //     auto bone_node = std::make_shared<BoneNode>(name);
        //     bone_node->SetResourceObjectRef(bone_name_map.at(name));
        //     scene->bone_nodes.emplace_back(bone_node);
        //     node = bone_node;
        // }
        // The node is empty
        else {
            // No thing to do;
        }

        for (std::size_t i = 0; i < _node->mNumChildren; i++) {
            convert(_node->mChildren[i], node);
        }
    };
    convert(ai_scene->mRootNode, ecs::Entity::InvalidEntity());
    logger->info("[Assimp] Parsing scnene graph costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    clock.Tick();

    logger->info("[Assimp] Processing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.TotalTime()).count());
}

}  // namespace hitagi::resource