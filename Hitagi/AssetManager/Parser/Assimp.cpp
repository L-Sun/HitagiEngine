#include "Assimp.hpp"

#include "HitagiMath.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

namespace Hitagi::Asset {

Scene AssimpParser::Parse(const Core::Buffer& buffer) {
    auto logger = spdlog::get("SceneManager");
    if (buffer.Empty()) {
        logger->warn("[Assimp] Parsing a empty buffer");
        return {};
    }

    auto  begin = std::chrono::high_resolution_clock::now();
    Scene scene;

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

    // process camera
    std::unordered_map<std::string_view, unsigned> camera_name_map;
    for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
        const auto _camera = ai_scene->mCameras[i];
        auto       perspective_camera =
            std::make_shared<Camera>(
                _camera->mAspect,
                _camera->mClipPlaneNear,
                _camera->mClipPlaneFar,
                _camera->mHorizontalFOV);

        scene.cameras[_camera->mName.C_Str()]   = perspective_camera;
        camera_name_map[_camera->mName.C_Str()] = i;
    }

    // process light
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
        scene.lights[_light->mName.C_Str()] = light;
    }

    // process material
    for (size_t i = 0; i < ai_scene->mNumMaterials; i++) {
        const auto _material = ai_scene->mMaterials[i];
        auto       material  = std::make_shared<Material>(_material->GetName().C_Str());
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

        scene.materials[_material->GetName().C_Str()] = material;
    }

    auto create_mesh = [&](const aiMesh* ai_mesh) -> Mesh {
        Mesh mesh;
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

        const std::string materialRef = ai_scene->mMaterials[ai_mesh->mMaterialIndex]->GetName().C_Str();
        mesh.SetMaterial(scene.materials.at(materialRef));
        return mesh;
    };

    auto get_matrix = [](const aiMatrix4x4& _mat) -> mat4f {
        mat4f ret;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                ret[i][j] = _mat[i][j];
        return ret;
    };

    auto create_geometry = [&](const aiNode* _node) -> std::shared_ptr<Geometry> {
        auto geometry = std::make_shared<Geometry>();
        for (size_t i = 0; i < _node->mNumMeshes; i++) {
            auto ai_mesh = ai_scene->mMeshes[_node->mMeshes[i]];
            geometry->AddMesh(create_mesh(ai_mesh));
        }
        return geometry;
    };

    std::function<std::shared_ptr<SceneNode>(const aiNode*)>
        convert = [&](const aiNode* _node) -> std::shared_ptr<SceneNode> {
        std::shared_ptr<SceneNode> node;
        const std::string          name(_node->mName.C_Str());
        // The node is a geometry
        if (_node->mNumMeshes > 0) {
            scene.geometries[name] = create_geometry(_node);

            auto geometry_node = std::make_shared<GeometryNode>(name);
            geometry_node->SetSceneObjectRef(scene.GetGeometry(name));
            scene.geometry_nodes[name] = geometry_node;
            node                       = geometry_node;
        }
        // The node is a camera
        else if (scene.cameras.count(name) != 0) {
            auto& _camera = ai_scene->mCameras[camera_name_map[name]];
            // move space infomation to camera node
            auto cameraNode = std::make_shared<CameraNode>(
                name,
                vec3f(_camera->mPosition.x, _camera->mPosition.y, _camera->mPosition.z),
                vec3f(_camera->mUp.x, _camera->mUp.y, _camera->mUp.z),
                vec3f(_camera->mLookAt.x, _camera->mLookAt.y, _camera->mLookAt.z));
            cameraNode->SetSceneObjectRef(scene.GetCamera(name));

            scene.camera_nodes[name] = cameraNode;
            node                     = cameraNode;
        }
        // The node is a light
        else if (scene.lights.count(name) != 0) {
            auto lightNode = std::make_shared<LightNode>(name);
            lightNode->SetSceneObjectRef(scene.GetLight(name));
            scene.light_nodes[name] = lightNode;
            node                    = lightNode;
        }
        // The node is empty
        else {
            node = std::make_shared<SceneEmptyNode>(name);
        }

        // Add transform matrix
        node->AppendTransform(get_matrix(_node->mTransformation));
        for (size_t i = 0; i < _node->mNumChildren; i++) {
            node->AppendChild(convert(_node->mChildren[i]));
        }
        return node;
    };

    scene.scene_graph = convert(ai_scene->mRootNode);

    end = std::chrono::high_resolution_clock::now();
    logger->info("[Assimp] Processing costs {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
    return scene;
}

}  // namespace Hitagi::Asset