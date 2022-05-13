#include <hitagi/parser/assimp.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>
#include <hitagi/resource/material_instance.hpp>
#include <hitagi/resource/bone.hpp>
#include <hitagi/math/transform.hpp>

#include <assimp/Importer.hpp>
#include "hitagi/resource/enums.hpp"
#include "hitagi/resource/mesh.hpp"
#include "magic_enum.hpp"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::resource {

void AssimpParser::Parse(Scene& scene, const core::Buffer& buffer, allocator_type alloc) {
    auto logger = spdlog::get("AssetManager");

    if (buffer.Empty()) {
        logger->warn("[Assimp] Parsing a empty buffer");
        return;
    }

    auto begin = std::chrono::high_resolution_clock::now();

    Assimp::Importer importer;
    auto             flag =
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_GenSmoothNormals |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices;

    const aiScene* ai_scene = importer.ReadFileFromMemory(buffer.GetData(), buffer.GetDataSize(), flag);
    if (!ai_scene) {
        logger->error("[Assimp] Can not parse the scene.");
        return;
    }
    auto end = std::chrono::high_resolution_clock::now();
    logger->info("[Assimp] Parsing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
    begin = std::chrono::high_resolution_clock::now();

    constexpr auto get_matrix = [](const aiMatrix4x4& _mat) -> const mat4f {
        mat4f ret(reinterpret_cast<const ai_real*>(&_mat));
        return transpose(ret);
    };

    constexpr auto get_vec3 = [](const aiVector3D& v) -> const vec3f {
        return vec3f(v.x, v.y, v.z);
    };
    constexpr auto get_color = [](const aiColor4D& c) -> const vec4f {
        return vec4f(c.r, c.g, c.b, c.a);
    };

    // process camera
    std::unordered_map<std::string_view, std::shared_ptr<Camera>> camera_name_map;
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];

        auto perspective_camera =
            std::allocate_shared<Camera>(
                alloc,
                _camera->mAspect,
                _camera->mClipPlaneNear,
                _camera->mClipPlaneFar,
                _camera->mHorizontalFOV,
                get_vec3(_camera->mPosition),
                get_vec3(_camera->mUp),
                get_vec3(_camera->mLookAt));

        perspective_camera->SetName(_camera->mName.C_Str());

        auto&& [result, success] = camera_name_map.emplace(perspective_camera->GetName(), perspective_camera);
        if (!success)
            logger->warn("A camera[{}] with the same name already exists!", perspective_camera->GetName());
    }

    // process light
    std::pmr::unordered_map<std::string_view, std::shared_ptr<Light>> light_name_map{alloc};
    for (size_t i = 0; i < ai_scene->mNumLights; i++) {
        const auto             _light = ai_scene->mLights[i];
        std::shared_ptr<Light> light;
        switch (_light->mType) {
            case aiLightSourceType::aiLightSource_AMBIENT:
                std::cerr << "[AssimpParser] " << std::endl;
                logger->warn("[Assimp] Unsupport light type: AMBIEN");
                break;
            case aiLightSourceType::aiLightSource_AREA:
                logger->warn("[Assimp] Unsupport light type: AREA");
                break;
            case aiLightSourceType::aiLightSource_DIRECTIONAL:
                logger->warn("[Assimp] Unsupport light type: DIRECTIONAL");
                break;
            case aiLightSourceType::aiLightSource_POINT: {
                vec4f color(1.0f);
                color.rgb       = normalize(vec3f(_light->mColorDiffuse.r, _light->mColorDiffuse.g, _light->mColorDiffuse.b));
                float intensity = _light->mColorDiffuse.r / color.r;
                light           = std::allocate_shared<PointLight>(alloc, color, intensity);
            } break;
            case aiLightSourceType::aiLightSource_SPOT: {
                vec4f diffuseColor(_light->mColorDiffuse.r, _light->mColorDiffuse.g, _light->mColorDiffuse.b, 1.0f);
                float intensity = _light->mColorDiffuse.r / diffuseColor.r;
                vec3f direction(_light->mDirection.x, _light->mDirection.y, _light->mDirection.z);
                light = std::allocate_shared<SpotLight>(
                    alloc,
                    diffuseColor,
                    intensity,
                    direction,
                    _light->mAngleInnerCone,
                    _light->mAngleOuterCone);
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
        light->SetName(_light->mName.C_Str());
        auto&& [result, success] = light_name_map.emplace(light->GetName(), light);
        if (!success)
            logger->warn("A light[{}] with the same name already exists!", light->GetName());
    }

    // process material
    // TODO support other material type
    auto                                                                         phong = scene.GetMaterial(MaterialType::Phong);
    std::pmr::unordered_map<std::string_view, std::shared_ptr<MaterialInstance>> material_name_map{alloc};
    for (std::size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        const auto _material = ai_scene->mMaterials[i];
        auto       material  = phong->CreateInstance();

        // set material diffuse color
        if (aiColor3D ambientColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor))
            material->SetParameter("ambient", vec4f(ambientColor.r, ambientColor.g, ambientColor.b, 1.0f));
        if (aiColor3D diffuseColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
            material->SetParameter("diffuse", vec4f(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f));
        // set material specular color
        if (aiColor3D specularColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor))
            material->SetParameter("specular", vec4f(specularColor.r, specularColor.g, specularColor.b, 1.0f));

        // TODO support the following parameter
        // // set material emission color
        // if (aiColor3D emissiveColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor))
        //     material->SetParameter("emission", vec4f(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f));
        // // set material transparent color
        // if (aiColor3D transparentColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_TRANSPARENT, transparentColor))
        //     material->SetParameter("transparency", vec4f(transparentColor.r, transparentColor.g, transparentColor.b, 1.0f));
        // // set material shiness
        // if (float shininess = 0; AI_SUCCESS == _material->Get(AI_MATKEY_SHININESS, shininess))
        //     material->SetParameter("specular_power", shininess);
        // // set material opacity
        // if (float opacity = 0; AI_SUCCESS == _material->Get(AI_MATKEY_OPACITY, opacity))
        //     material->SetParameter("opacity", opacity);

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
            },
            alloc};
        for (auto&& [key1, key2] : map) {
            for (size_t i = 0; i < _material->GetTextureCount(key2); i++) {
                aiString _path;
                if (AI_SUCCESS == _material->GetTexture(aiTextureType::aiTextureType_DIFFUSE, i, &_path)) {
                    std::filesystem::path path(_path.C_Str());
                    auto                  texture = std::allocate_shared<Texture>(alloc, path);
                    texture->SetName(_path.C_Str());
                    material->SetTexture(key1, texture);
                }
                break;  // unsupport blend for now.
            }
        }

        // set material name
        if (aiString name; AI_SUCCESS == _material->Get(AI_MATKEY_NAME, name))
            material->SetName(name.C_Str());

        auto&& [result, success] = material_name_map.emplace(material->GetName(), material);
        if (!success)
            logger->warn("A material[{}] with the same name already exists!", material->GetName());
    }

    std::pmr::unordered_map<std::string, std::shared_ptr<Bone>> bone_name_map{alloc};
    std::pmr::unordered_map<aiMesh*, Mesh>                      meshes{alloc};

    auto covert_mesh = [&](const aiMesh* ai_mesh) -> Mesh {
        // Mesh mesh;
        // mesh.SetName(ai_mesh->mName.C_Str());
        // // Set primitive type

        auto vertices = std::allocate_shared<VertexArray>(alloc, ai_mesh->mNumVertices);

        // Read Position
        if (ai_mesh->HasPositions()) {
            auto       position  = vertices->GetVertices<VertexAttribute::Position>();
            const auto _position = std::span<aiVector3D>(ai_mesh->mVertices, ai_mesh->mNumVertices);
            std::transform(_position.begin(), _position.end(), position.begin(), get_vec3);
        }

        // Read Normal
        if (ai_mesh->HasNormals()) {
            auto       normal  = vertices->GetVertices<VertexAttribute::Normal>();
            const auto _normal = std::span<aiVector3D>(ai_mesh->mNormals, ai_mesh->mNumVertices);
            std::transform(_normal.begin(), _normal.end(), normal.begin(), get_vec3);
        }

        // Read Color
        {
            if (ai_mesh->GetNumColorChannels() >= 4) {
                logger->warn("Only 4 color channels are supported now!");
            }
            constexpr std::array channels    = {VertexAttribute::Color0, VertexAttribute::Color1, VertexAttribute::Color2, VertexAttribute::Color3};
            auto                 build_color = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                if (ai_mesh->HasVertexColors(I)) {
                    auto       color  = vertices->GetVertices<channels.at(I)>();
                    const auto _color = std::span<aiColor4D>(ai_mesh->mColors[I], ai_mesh->mNumVertices);
                    std::transform(_color.begin(), _color.end(), color.begin(), get_color);
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
                    auto       uv  = vertices->GetVertices<channels.at(I)>();
                    const auto _uv = std::span<aiVector3D>(ai_mesh->mTextureCoords[I], ai_mesh->mNumVertices);
                    std::transform(_uv.begin(), _uv.end(), uv.begin(), [](const aiVector3D& uv) { return vec2f(uv.x, uv.y); });
                }
            };
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (build_color(std::integral_constant<std::size_t, I>{}), ...);
            }
            (std::make_index_sequence<4>{});
        }

        // Read Tangent and Bitangent
        if (ai_mesh->HasTangentsAndBitangents()) {
            auto       tangent  = vertices->GetVertices<VertexAttribute::Tangent>();
            const auto _tangent = std::span<aiVector3D>(ai_mesh->mTangents, ai_mesh->mNumVertices);
            std::transform(_tangent.begin(), _tangent.end(), tangent.begin(), get_vec3);

            auto       bi_tangent  = vertices->GetVertices<VertexAttribute::Bitangent>();
            const auto _bi_tangent = std::span<aiVector3D>(ai_mesh->mBitangents, ai_mesh->mBitangents);
            std::transform(_bi_tangent.begin(), _bi_tangent.end(), bi_tangent.begin(), get_vec3);
        }

        // Read Indices
        std::size_t num_indices = 0;
        for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++)
            num_indices += ai_mesh->mFaces[face].mNumIndices;

        auto        indices       = std::allocate_shared<IndexArray>(alloc, num_indices, IndexType::UINT32);
        auto        indices_array = indices->GetIndices<IndexType::UINT32>();
        std::size_t p             = 0;
        for (std::size_t face = 0; face < ai_mesh->mNumFaces; face++)
            for (std::size_t i = 0; i < ai_mesh->mFaces[face].mNumIndices; i++)
                indices_array[p++] = ai_mesh->mFaces[face].mIndices[i];

        PrimitiveType primitive;
        switch (ai_mesh->mPrimitiveTypes) {
            case aiPrimitiveType::aiPrimitiveType_LINE:
                primitive = PrimitiveType::LineList;
                break;
            case aiPrimitiveType::aiPrimitiveType_POINT:
                primitive = PrimitiveType::PointList;
                break;
            case aiPrimitiveType::aiPrimitiveType_TRIANGLE:
                primitive = PrimitiveType::TriangleList;
                break;
            case aiPrimitiveType::aiPrimitiveType_POLYGON:
            case aiPrimitiveType::_aiPrimitiveType_Force32Bit:
            default:
                primitive = PrimitiveType::Unkown;
                logger->error("[Assimp] Unsupport Primitive Type");
        }

        // material
        std::shared_ptr<MaterialInstance> material      = nullptr;
        const std::string_view            material_name = ai_scene->mMaterials[ai_mesh->mMaterialIndex]->GetName().C_Str();
        if (material_name_map.count(material_name) != 0)
            material = material_name_map.at(material_name);
        else
            logger->warn("A mesh[{}] refers to a unexist material", material_name);

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

        return {vertices, indices, material, primitive, 0, 0, 0, alloc};
    };

    for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
        auto ai_mesh = ai_scene->mMeshes[i];
        meshes.emplace(ai_mesh, covert_mesh(ai_mesh));
    }

    auto create_geometry = [&](const aiNode* _node, const std::shared_ptr<Transform>& transform) -> Geometry {
        Geometry geometry(transform, alloc);
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            auto ai_mesh     = ai_scene->mMeshes[_node->mMeshes[i]];
            auto mesh_handle = meshes.extract(ai_mesh);
            assert(!mesh_handle.empty());
            geometry.AddMesh(std::move(mesh_handle.mapped()));
        }
        return geometry;
    };

    std::function<void(const aiNode*, const std::shared_ptr<Transform>&)>
        convert = [&](const aiNode* _node, const std::shared_ptr<Transform>& parent) -> void {
        std::string_view name = _node->mName.C_Str();

        auto transform = std::allocate_shared<Transform>(alloc, decompose(get_matrix(_node->mTransformation)));
        transform->SetParent(parent);

        // The node is a geometry
        if (_node->mNumMeshes > 0) {
            auto geometry = create_geometry(_node, transform);
            geometry.SetName(name);
            scene.AddGeometry(std::move(geometry));
        }
        // The node is a camera
        else if (camera_name_map.count(name) != 0) {
            camera_name_map.at(name)->SetTransform(transform);
        }
        // The node is a light
        else if (light_name_map.count(name) != 0) {
            scene.AddLight(light_name_map.at(name));
        }
        // // The node is bone
        // else if (bone_name_map.count(name) != 0) {
        //     auto bone_node = std::make_shared<BoneNode>(name);
        //     bone_node->SetSceneObjectRef(bone_name_map.at(name));
        //     scene->bone_nodes.emplace_back(bone_node);
        //     node = bone_node;
        // }
        // The node is empty
        else {
            // No thing to do;
        }
    };

    end = std::chrono::high_resolution_clock::now();
    logger->info("[Assimp] Processing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
}

}  // namespace hitagi::resource