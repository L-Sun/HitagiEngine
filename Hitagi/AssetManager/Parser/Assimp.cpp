#include "Assimp.hpp"

#include "HitagiMath.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

namespace Hitagi::Asset {

std::shared_ptr<Scene> AssimpParser::Parse(const Core::Buffer& buffer) {
    auto logger = spdlog::get("AssetManager");
    auto scene  = std::make_shared<Scene>();

    if (buffer.Empty()) {
        logger->warn("[Assimp] Parsing a empty buffer");
        return scene;
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
        return scene;
    }
    auto end = std::chrono::high_resolution_clock::now();
    logger->info("[Assimp] Parsing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
    begin = std::chrono::high_resolution_clock::now();

    auto get_matrix = [](const aiMatrix4x4& _mat) -> const mat4f {
        mat4f ret;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                ret[i][j] = _mat[i][j];
        return ret;
    };

    // process camera
    std::unordered_map<std::string_view, std::pair<unsigned, std::shared_ptr<Camera>>> camera_name_map;
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];
        auto       perspective_camera =
            std::make_shared<Camera>(
                _camera->mAspect,
                _camera->mClipPlaneNear,
                _camera->mClipPlaneFar,
                _camera->mHorizontalFOV);
        perspective_camera->SetName(_camera->mName.C_Str());

        auto&& [result, success] = camera_name_map.emplace(perspective_camera->GetName(), std::make_pair(i, perspective_camera));
        if (!success)
            logger->warn("A camera[{}] with the same name already exists!", perspective_camera->GetName());

        scene->cameras.emplace_back(perspective_camera);
    }

    // process light
    std::unordered_map<std::string_view, std::shared_ptr<Light>> light_name_map;
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
                light           = std::make_shared<PointLight>(color, intensity);
            } break;
            case aiLightSourceType::aiLightSource_SPOT: {
                vec4f diffuseColor(_light->mColorDiffuse.r, _light->mColorDiffuse.g, _light->mColorDiffuse.b, 1.0f);
                float intensity = _light->mColorDiffuse.r / diffuseColor.r;
                vec3f direction(_light->mDirection.x, _light->mDirection.y, _light->mDirection.z);
                light = std::make_shared<SpotLight>(
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

        scene->lights.emplace_back(light);
    }

    // process material
    std::unordered_map<std::string_view, std::shared_ptr<Material>> material_name_map;
    for (size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        const auto _material = ai_scene->mMaterials[i];
        auto       material  = std::make_shared<Material>();
        // set material name
        if (aiString name; AI_SUCCESS == _material->Get(AI_MATKEY_NAME, name))
            material->SetName(name.C_Str());
        // set material diffuse color
        if (aiColor3D ambientColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor))
            material->SetColor("ambient", vec4f(ambientColor.r, ambientColor.g, ambientColor.b, 1.0f));
        if (aiColor3D diffuseColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
            material->SetColor("diffuse", vec4f(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f));
        // set material specular color
        if (aiColor3D specularColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor))
            material->SetColor("specular", vec4f(specularColor.r, specularColor.g, specularColor.b, 1.0f));
        // set material emission color
        if (aiColor3D emissiveColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor))
            material->SetColor("emission", vec4f(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f));
        // set material transparent color
        if (aiColor3D transparentColor; AI_SUCCESS == _material->Get(AI_MATKEY_COLOR_TRANSPARENT, transparentColor))
            material->SetColor("transparency", vec4f(transparentColor.r, transparentColor.g, transparentColor.b, 1.0f));
        // set material shiness
        if (float shininess = 0; AI_SUCCESS == _material->Get(AI_MATKEY_SHININESS, shininess))
            material->SetParam("specular_power", shininess);
        // set material opacity
        if (float opacity = 0; AI_SUCCESS == _material->Get(AI_MATKEY_OPACITY, opacity))
            material->SetParam("opacity", opacity);

        // set diffuse texture
        // TODO: blend mutiple texture
        const std::unordered_map<std::string, aiTextureType> map = {
            {"diffuse", aiTextureType::aiTextureType_DIFFUSE},
            {"specular", aiTextureType::aiTextureType_SPECULAR},
            {"emission", aiTextureType::aiTextureType_EMISSIVE},
            {"opacity", aiTextureType::aiTextureType_OPACITY},
            // {"transparency", },
            {"normal", aiTextureType::aiTextureType_NORMALS},
        };
        for (auto&& [key1, key2] : map) {
            for (size_t i = 0; i < _material->GetTextureCount(key2); i++) {
                aiString _path;
                if (AI_SUCCESS == _material->GetTexture(aiTextureType::aiTextureType_DIFFUSE, i, &_path)) {
                    std::filesystem::path path(_path.C_Str());
                    material->SetTexture(key1, std::make_shared<Texture>(path));
                }
                break;  // unsupport blend for now.
            }
        }

        auto&& [result, success] = material_name_map.emplace(material->GetName(), material);
        if (!success)
            logger->warn("A material[{}] with the same name already exists!", material->GetName());

        scene->materials.emplace_back(material);
    }

    std::unordered_map<std::string, std::shared_ptr<Bone>> bone_name_map;
    std::unordered_map<aiMesh*, Mesh>                      meshes;

    auto covert_mesh = [&](const aiMesh* ai_mesh) -> Mesh {
        Mesh mesh;
        mesh.SetName(ai_mesh->mName.C_Str());
        // Set primitive type
        switch (ai_mesh->mPrimitiveTypes) {
            case aiPrimitiveType::aiPrimitiveType_LINE:
                mesh.SetPrimitiveType(PrimitiveType::LineList);
                break;
            case aiPrimitiveType::aiPrimitiveType_POINT:
                mesh.SetPrimitiveType(PrimitiveType::PointList);
                break;
            case aiPrimitiveType::aiPrimitiveType_TRIANGLE:
                mesh.SetPrimitiveType(PrimitiveType::TriangleList);
                break;
            case aiPrimitiveType::aiPrimitiveType_POLYGON:
            case aiPrimitiveType::_aiPrimitiveType_Force32Bit:
            default:
                logger->error("[Assimp] Unsupport Primitive Type");
        }

        // Read Position
        if (ai_mesh->HasPositions()) {
            Core::Buffer positionBuffer(ai_mesh->mNumVertices * sizeof(vec3f));
            auto         position = reinterpret_cast<vec3f*>(positionBuffer.GetData());
            for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                position[i] = vec3f(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            mesh.AddVertexArray(VertexArray("POSITION", VertexArray::DataType::Float3, std::move(positionBuffer)));
        }

        // Read Normal
        if (ai_mesh->HasNormals()) {
            Core::Buffer normalBuffer(ai_mesh->mNumVertices * sizeof(vec3f));
            auto         normal = reinterpret_cast<vec3f*>(normalBuffer.GetData());
            for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                normal[i] = vec3f(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
            mesh.AddVertexArray(VertexArray("NORMAL", VertexArray::DataType::Float3, std::move(normalBuffer)));
        }

        // Read Color
        for (size_t colorChannels = 0; colorChannels < ai_mesh->GetNumColorChannels(); colorChannels++) {
            if (ai_mesh->HasVertexColors(colorChannels)) {
                Core::Buffer colorBuffer(ai_mesh->mNumVertices * sizeof(vec4f));
                auto         color = reinterpret_cast<vec4f*>(colorBuffer.GetData());
                for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                    color[i] = vec4f(ai_mesh->mColors[colorChannels][i].r,
                                     ai_mesh->mColors[colorChannels][i].g,
                                     ai_mesh->mColors[colorChannels][i].b,
                                     ai_mesh->mColors[colorChannels][i].a);
                const auto attr = std::string("COLOR") + (colorChannels == 0 ? "" : std::to_string(colorChannels));
                mesh.AddVertexArray(VertexArray(attr, VertexArray::DataType::Float4, std::move(colorBuffer)));
            }
        }

        // Read UV
        for (size_t UVChannel = 0; UVChannel < ai_mesh->GetNumUVChannels(); UVChannel++) {
            if (ai_mesh->HasTextureCoords(UVChannel)) {
                Core::Buffer texcoordBuffer(ai_mesh->mNumVertices * sizeof(vec2f));
                auto         texcoord = reinterpret_cast<vec2f*>(texcoordBuffer.GetData());
                for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                    texcoord[i] = vec2f(ai_mesh->mTextureCoords[UVChannel][i].x, ai_mesh->mTextureCoords[UVChannel][i].y);

                const auto attr = std::string("TEXCOORD") + (UVChannel == 0 ? "" : std::to_string(UVChannel));
                mesh.AddVertexArray(VertexArray(attr, VertexArray::DataType::Float2, std::move(texcoordBuffer)));
            }
        }

        // Read Tangent and Bitangent
        if (ai_mesh->HasTangentsAndBitangents()) {
            Core::Buffer tangentBuffer(ai_mesh->mNumVertices * sizeof(vec3f));
            auto         tangent = reinterpret_cast<vec3f*>(tangentBuffer.GetData());
            for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                tangent[i] = vec3f(ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z);
            mesh.AddVertexArray(VertexArray("TANGENT", VertexArray::DataType::Float3, std::move(tangentBuffer)));

            Core::Buffer bitangentBuffer(ai_mesh->mNumVertices * sizeof(vec3f));
            auto         bitangent = reinterpret_cast<vec3f*>(bitangentBuffer.GetData());
            for (size_t i = 0; i < ai_mesh->mNumVertices; i++)
                bitangent[i] = vec3f(ai_mesh->mBitangents[i].x, ai_mesh->mBitangents[i].y, ai_mesh->mBitangents[i].z);
            mesh.AddVertexArray(VertexArray("BITANGENT", VertexArray::DataType::Float3, std::move(bitangentBuffer)));
        }

        // Read Indices
        size_t indicesCount = 0;
        for (size_t face = 0; face < ai_mesh->mNumFaces; face++)
            indicesCount += ai_mesh->mFaces[face].mNumIndices;

        Core::Buffer indexBuffer(indicesCount * sizeof(int));
        auto         indices = reinterpret_cast<int*>(indexBuffer.GetData());
        for (size_t face = 0; face < ai_mesh->mNumFaces; face++)
            for (size_t i = 0; i < ai_mesh->mFaces[face].mNumIndices; i++)
                *indices++ = ai_mesh->mFaces[face].mIndices[i];  // assignment then increase

        mesh.SetIndexArray(IndexArray(IndexArray::DataType::Int32, std::move(indexBuffer)));

        // material
        const std::string_view material_name = ai_scene->mMaterials[ai_mesh->mMaterialIndex]->GetName().C_Str();
        if (material_name_map.count(material_name) != 0)
            mesh.SetMaterial(material_name_map.at(material_name));
        else
            logger->warn("A mesh[{}] refers to a unexist material", material_name);

        // bone
        if (ai_mesh->HasBones()) {
            for (size_t bone_index = 0; bone_index < ai_mesh->mNumBones; bone_index++) {
                auto _bone = ai_mesh->mBones[bone_index];
                auto bone  = mesh.CreateNewBone(_bone->mName.C_Str());
                bone_name_map.emplace(bone->GetName(), bone);

                bone->SetName(_bone->mName.C_Str());
                for (size_t i = 0; i < _bone->mNumWeights; i++)
                    bone->SetWeight(_bone->mWeights[i].mVertexId, _bone->mWeights[i].mWeight);
                bone->SetBindTransformMatrix(get_matrix(_bone->mOffsetMatrix));
            }
        }

        return mesh;
    };

    for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
        auto ai_mesh = ai_scene->mMeshes[i];
        meshes.emplace(ai_mesh, covert_mesh(ai_mesh));
    }

    auto create_geometry = [&](const aiNode* _node) -> std::shared_ptr<Geometry> {
        auto geometry = std::make_shared<Geometry>();
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            auto ai_mesh     = ai_scene->mMeshes[_node->mMeshes[i]];
            auto mesh_handle = meshes.extract(ai_mesh);
            assert(!mesh_handle.empty());
            geometry->AddMesh(std::move(mesh_handle.mapped()));
        }
        return geometry;
    };

    std::function<std::shared_ptr<SceneNode>(const aiNode*, std::weak_ptr<SceneNode>)>
        convert = [&](const aiNode* _node, std::weak_ptr<SceneNode> parent) -> std::shared_ptr<SceneNode> {
        std::shared_ptr<SceneNode> node;
        const std::string          name(_node->mName.C_Str());
        // The node is a geometry
        if (_node->mNumMeshes > 0) {
            auto geometry = create_geometry(_node);
            scene->geometries.emplace_back(geometry);

            auto geometry_node = std::make_shared<GeometryNode>(name);
            geometry_node->SetSceneObjectRef(geometry);
            scene->geometry_nodes.emplace_back(geometry_node);
            node = geometry_node;
        }
        // The node is a camera
        else if (camera_name_map.count(name) != 0) {
            auto& _camera = ai_scene->mCameras[camera_name_map[name].first];
            // move space infomation to camera node
            auto camera_node = std::make_shared<CameraNode>(
                name,
                vec3f(_camera->mPosition.x, _camera->mPosition.y, _camera->mPosition.z),
                vec3f(_camera->mUp.x, _camera->mUp.y, _camera->mUp.z),
                vec3f(_camera->mLookAt.x, _camera->mLookAt.y, _camera->mLookAt.z));
            camera_node->SetSceneObjectRef(camera_name_map[name].second);

            scene->camera_nodes.emplace_back(camera_node);
            node = camera_node;
        }
        // The node is a light
        else if (light_name_map.count(name) != 0) {
            auto light_node = std::make_shared<LightNode>(name);
            light_node->SetSceneObjectRef(light_name_map.at(name));
            scene->light_nodes.emplace_back(light_node);
            node = light_node;
        }
        // The node is bone
        else if (bone_name_map.count(name) != 0) {
            auto bone_node = std::make_shared<BoneNode>(name);
            bone_node->SetSceneObjectRef(bone_name_map.at(name));
            scene->bone_nodes.emplace_back(bone_node);
            node = bone_node;
        }
        // The node is empty
        else {
            node = std::make_shared<SceneEmptyNode>(name);
        }

        // Add transform matrix
        node->ApplyTransform(get_matrix(_node->mTransformation));
        node->SetParent(parent);
        for (size_t i = 0; i < _node->mNumChildren; i++) {
            node->AppendChild(convert(_node->mChildren[i], node));
        }
        return node;
    };

    scene->scene_graph = convert(ai_scene->mRootNode, std::weak_ptr<SceneNode>{});

    end = std::chrono::high_resolution_clock::now();
    logger->info("[Assimp] Processing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
    return scene;
}

}  // namespace Hitagi::Asset