#include <iostream>
#include <fstream>
#include <cassert>
#include "OpenGLGraphicsManager.hpp"
#include "IApplication.hpp"
#include "AssetLoader.hpp"
#include "SceneManager.hpp"
#include "PhysicsManager.hpp"

const char VS_SHADER_SOURCE_FILE[] = "Shaders/basic.vs";
const char FS_SHADER_SOURCE_FILE[] = "Shaders/basic.fs";

using namespace std;

namespace My {

static void OutputShaderErrorMessage(unsigned int shaderId,
                                     const char*  shaderFileName) {
    int      logSize, i;
    char*    infoLog;
    ofstream fout;

    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logSize);
    logSize++;
    infoLog = new char[logSize];
    if (!infoLog) return;
    glGetShaderInfoLog(shaderId, logSize, NULL, infoLog);

    fout.open("shader-error.txt");

    for (i = 0; i < logSize; i++) {
        fout << infoLog[i];
    }
    fout.close();
    cerr << "Error compiling shader. Check shader-error.txt for message."
         << shaderFileName << endl;
    return;
}

static void OutputLinkerErrorMessage(unsigned int programId) {
    int      logSize, i;
    char*    infoLog;
    ofstream fout;
    glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logSize);
    logSize++;

    infoLog = new char[logSize];
    if (!infoLog) {
        return;
    }
    glGetProgramInfoLog(programId, logSize, NULL, infoLog);
    fout.open("link-error.txt");

    for (i = 0; i < logSize; i++) {
        fout << infoLog[i];
    }
    fout.close();
    cerr << "Error compiling linker. Check linker-error.txt for message."
         << endl;
}

int OpenGLGraphicsManager::Initialize() {
    int result;
    result = GraphicsManager::Initialize();
    if (result) {
        return result;
    }
    result = gladLoadGL();
    if (!result) {
        cout << "OpenGL load failed!\n";
        return -1;
    } else {
        result = 0;
        cout << "OpenGL Version" << glGetString(GL_VERSION) << endl;

        if (GLAD_GL_VERSION_3_0) {
            // Set the depth buffer to be entirely cleared to 1.0 values.
            glClearDepth(1.0f);

            // Enable depth testing.
            glEnable(GL_DEPTH_TEST);

            // Enable multisample
            glEnable(GL_MULTISAMPLE);

            // Set the polygon winding to front facing for the left handed
            // system.
            glFrontFace(GL_CCW);

            // Enable back face culling.
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }
        InitializeShader(VS_SHADER_SOURCE_FILE, FS_SHADER_SOURCE_FILE);
        InitializeBuffers();
    }

    return result;
}
void OpenGLGraphicsManager::Finalize() {
    for (auto dbc : m_DrawBatchContext) {
        glDeleteVertexArrays(1, &dbc.vao);
    }
    m_DrawBatchContext.clear();

    for (auto buf : m_Buffers) {
        glDeleteBuffers(1, &buf);
    }
    for (auto texture : m_Textures) {
        glDeleteTextures(1, &texture);
    }

    m_Buffers.clear();
    m_Textures.clear();

    if (m_shaderProgram) {
        if (m_vertexShader) {
            glDetachShader(m_shaderProgram, m_vertexShader);
            glDeleteShader(m_vertexShader);
        }
        if (m_fragmentShader) {
            glDetachShader(m_shaderProgram, m_fragmentShader);
            glDeleteShader(m_fragmentShader);
        }

        glDeleteProgram(m_shaderProgram);
    }

    GraphicsManager::Finalize();
}

void OpenGLGraphicsManager::Clear() {
    GraphicsManager::Clear();
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsManager::Draw() {
    GraphicsManager::Draw();
    RenderBuffers();
    glFlush();
}

bool OpenGLGraphicsManager::SetPerFrameShaderParameters() {
    unsigned int location;
    // Set world matrix
    location = glGetUniformLocation(m_shaderProgram, "worldMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, m_DrawFrameContext.m_worldMatrix);

    // Set view matrix
    location = glGetUniformLocation(m_shaderProgram, "viewMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, m_DrawFrameContext.m_viewMatrix);

    // Set projection matrix
    location = glGetUniformLocation(m_shaderProgram, "projectionMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false,
                       m_DrawFrameContext.m_projectionMatrix);

    location = glGetUniformLocation(m_shaderProgram, "lightPosition");
    if (location == -1) {
        return false;
    }
    glUniform3fv(location, 1, m_DrawFrameContext.m_lightPosition);

    location = glGetUniformLocation(m_shaderProgram, "lightColor");
    if (location == -1) {
        return false;
    }
    glUniform4fv(location, 1, m_DrawFrameContext.m_lightColor);

    return true;
}

bool OpenGLGraphicsManager::SetPerBatchShaderParameters(const char* paramName,
                                                        const mat4& param) {
    unsigned int location;
    location = glGetUniformLocation(m_shaderProgram, paramName);
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(const char* paramName,
                                                        const vec3& param) {
    unsigned int location;
    location = glGetUniformLocation(m_shaderProgram, paramName);
    if (location == -1) {
        return false;
    }
    glUniform3fv(location, 1, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(const char* paramName,
                                                        const float param) {
    unsigned int location;
    location = glGetUniformLocation(m_shaderProgram, paramName);
    if (location == -1) {
        return false;
    }
    glUniform1f(location, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(
    const char* paramName, const GLint texture_index) {
    unsigned int location;
    location = glGetUniformLocation(m_shaderProgram, paramName);
    if (location == -1) {
        return false;
    }

    if (texture_index < GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
        glUniform1i(location, texture_index);
    return true;
}

void OpenGLGraphicsManager::InitializeBuffers() {
    auto& scene = g_pSceneManager->GetSceneForRendering();

    for (auto _it : scene.GeometryNodes) {
        auto pGeometryNode = _it.second;
        if (pGeometryNode->Visible()) {
            auto pGeometry =
                scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
            assert(pGeometry);
            auto pMesh = pGeometry->GetMesh().lock();
            if (!pMesh) return;

            auto vertexPropertiesCount = pMesh->GetVertexPropertiesCount();
            auto vertexCount           = pMesh->GetVertexCount();

            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            GLuint buffer_id;

            for (int32_t i = 0; i < vertexPropertiesCount; i++) {
                const SceneObjectVertexArray& v_property_array =
                    pMesh->GetVertexPropertyArray(i);
                auto v_property_array_data_size =
                    v_property_array.GetDataSize();
                auto v_property_array_data = v_property_array.GetData();

                glGenBuffers(1, &buffer_id);
                glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
                glBufferData(GL_ARRAY_BUFFER, v_property_array_data_size,
                             v_property_array_data, GL_STATIC_DRAW);

                glEnableVertexAttribArray(i);

                switch (v_property_array.GetDataType()) {
                    case VertexDataType::FLOAT1:
                        glVertexAttribPointer(i, 1, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::FLOAT2:
                        glVertexAttribPointer(i, 2, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::FLOAT3:
                        glVertexAttribPointer(i, 3, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::FLOAT4:
                        glVertexAttribPointer(i, 4, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::DOUBLE1:
                        glVertexAttribPointer(i, 1, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::DOUBLE2:
                        glVertexAttribPointer(i, 2, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::DOUBLE3:
                        glVertexAttribPointer(i, 3, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::DOUBLE4:
                        glVertexAttribPointer(i, 4, GL_DOUBLE, false, 0, 0);
                        break;
                    default:
                        assert(0);
                        break;
                }
                m_Buffers.push_back(buffer_id);
            }

            auto indexGroupCount = pMesh->GetIndexGroupCount();

            GLenum mode;
            switch (pMesh->GetPrimitiveType()) {
                case PrimitiveType::POINT_LIST:
                    mode = GL_POINTS;
                    break;
                case PrimitiveType::LINE_LIST:
                    mode = GL_LINES;
                    break;
                case PrimitiveType::LINE_STRIP:
                    mode = GL_LINE_STRIP;
                    break;
                case PrimitiveType::TRI_LIST:
                    mode = GL_TRIANGLES;
                    break;
                case PrimitiveType::TRI_STRIP:
                    mode = GL_TRIANGLE_STRIP;
                    break;
                case PrimitiveType::TRI_FAN:
                    mode = GL_TRIANGLE_FAN;
                    break;
                default:
                    continue;
            }

            for (decltype(indexGroupCount) i = 0; i < indexGroupCount; i++) {
                glGenBuffers(1, &buffer_id);

                const SceneObjectIndexArray& index_array =
                    pMesh->GetIndexArray(i);
                auto index_array_size = index_array.GetDataSize();
                auto index_array_data = index_array.GetData();

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array_size,
                             index_array_data, GL_STATIC_DRAW);
                GLsizei indexCount =
                    static_cast<GLsizei>(index_array.GetIndexCount());
                GLenum type;
                switch (index_array.GetIndexType()) {
                    case IndexDataType::INT8:
                        type = GL_UNSIGNED_BYTE;
                        break;
                    case IndexDataType::INT16:
                        type = GL_UNSIGNED_SHORT;
                        break;
                    case IndexDataType::INT32:
                        type = GL_UNSIGNED_INT;
                        break;
                    default:
                        // not supported by OpenGL
                        cout << "Error: Unsupported Index Type " << index_array
                             << endl;
                        cout << "Mesh: " << *pMesh << endl;
                        cout << "Geometry: " << *pGeometry << endl;
                        continue;
                }

                m_Buffers.push_back(buffer_id);

                size_t      material_index = index_array.GetMaterialIndex();
                std::string material_key =
                    pGeometryNode->GetMaterialRef(material_index);
                auto material = scene.GetMaterial(material_key);

                if (material) {
                    auto color = material->GetBaseColor();
                    if (color.ValueMap) {
                        auto texture = color.ValueMap->GetTextureImage();
                        auto it      = m_TextureIndex.find(material_key);
                        if (it == m_TextureIndex.end()) {
                            GLuint texture_id;
                            glGenTextures(1, &texture_id);
                            glActiveTexture(GL_TEXTURE0 + texture_id);
                            glBindTexture(GL_TEXTURE_2D, texture_id);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                                         texture.Width, texture.Height, 0,
                                         GL_RGBA, GL_UNSIGNED_BYTE,
                                         texture.data);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                            GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                            GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D,
                                            GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D,
                                            GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                            m_TextureIndex[color.ValueMap->GetName()] =
                                texture_id;
                            m_Textures.push_back(texture_id);
                        }
                    }
                }

                DrawBatchContext& dbc = *(new DrawBatchContext);
                dbc.vao               = vao;
                dbc.mode              = mode;
                dbc.type              = type;
                dbc.node              = pGeometryNode;
                dbc.material          = material;
                dbc.count             = indexCount;
                m_DrawBatchContext.push_back(std::move(dbc));
            }
        }
        pGeometryNode = scene.GetNextGeometryNode();
    }
    return;
}

void OpenGLGraphicsManager::RenderBuffers() {
    SetPerFrameShaderParameters();

    for (auto dbc : m_DrawBatchContext) {
        glUseProgram(m_shaderProgram);

        mat4 trans = *dbc.node->GetCalculatedTransform();

        if (void* rigidBody = dbc.node->RigidBody()) {
            // the geometry has rigid body bounded, we blend the simlation
            // result here.
            mat4 simulated_result =
                g_pPhysicsManager->GetRigidBodyTransform(rigidBody);

            // reset the translation part of the matrix
            memcpy(trans[3], vec3(0.0f, 0.0f, 0.0f), sizeof(float) * 3);

            // apply the rotation part of the simlation result
            mat4 rotation(1.0f);
            memcpy(rotation[0], simulated_result[0], sizeof(float) * 3);
            memcpy(rotation[1], simulated_result[1], sizeof(float) * 3);
            memcpy(rotation[2], simulated_result[2], sizeof(float) * 3);
            trans = trans * rotation;

            // replace the translation part of the matrix with simlation result
            // directly
            memcpy(trans[3], simulated_result[3], sizeof(float) * 3);
        }
        SetPerBatchShaderParameters("modelMatrix", trans);
        glBindVertexArray(dbc.vao);
        // auto           indexBufferCount = dbc.counts.size();
        // const GLvoid** pIndicies        = new const
        // GLvoid*[indexBufferCount]; memset(pIndicies, 0x00, sizeof(GLvoid*) *
        // indexBufferCount); glMultiDrawElements(dbc.mode, dbc.counts.data(),
        // dbc.type, pIndicies, indexBufferCount);
        // delete[] pIndicies;

        if (dbc.material) {
            Color color = dbc.material->GetBaseColor();

            if (color.ValueMap) {
                SetPerBatchShaderParameters(
                    "defaultSampler",
                    m_TextureIndex[color.ValueMap->GetName()]);
                SetPerBatchShaderParameters("diffuseColor", vec3(-1.0f));
            } else {
                SetPerBatchShaderParameters("diffuseColor", color.Value.rgb);
            }

            color = dbc.material->GetSpecularColor();
            SetPerBatchShaderParameters("specularColor", color.Value.rgb);

            Parameter param = dbc.material->GetSpecularPower();
            SetPerBatchShaderParameters("specularPower", param.Value);
        }

        glDrawElements(dbc.mode, dbc.count, dbc.type, 0x00);
    }
    return;
}

bool OpenGLGraphicsManager::InitializeShader(const char* vsFilename,
                                             const char* fsFilename) {
    string vertexShaderBuffer;
    string fragmentShaderBuffer;
    int    status;

    vertexShaderBuffer =
        g_pAssetLoader->SyncOpenAndReadTextFileToString(vsFilename);
    if (vertexShaderBuffer.empty()) {
        return false;
    }
    fragmentShaderBuffer =
        g_pAssetLoader->SyncOpenAndReadTextFileToString(fsFilename);
    if (fragmentShaderBuffer.empty()) {
        return false;
    }

    m_vertexShader   = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    const char* _v_c_str = vertexShaderBuffer.c_str();
    const char* _f_c_str = fragmentShaderBuffer.c_str();
    glShaderSource(m_vertexShader, 1, &_v_c_str, NULL);
    glShaderSource(m_fragmentShader, 1, &_f_c_str, NULL);

    glCompileShader(m_vertexShader);
    glCompileShader(m_fragmentShader);

    glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &status);
    if (status != 1) {
        OutputShaderErrorMessage(m_vertexShader, vsFilename);
        return false;
    }
    glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &status);
    if (status != 1) {
        OutputShaderErrorMessage(m_fragmentShader, fsFilename);
        return false;
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, m_vertexShader);
    glAttachShader(m_shaderProgram, m_fragmentShader);

    glBindAttribLocation(m_shaderProgram, 0, "inputPosition");
    glBindAttribLocation(m_shaderProgram, 1, "inputNormal");
    glBindAttribLocation(m_shaderProgram, 2, "inputUV");

    glLinkProgram(m_shaderProgram);
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &status);
    if (status != 1) {
        OutputLinkerErrorMessage(m_shaderProgram);
        return false;
    }
    return true;
}

}  // namespace My