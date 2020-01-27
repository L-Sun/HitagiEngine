#include <iostream>
#include <fstream>
#include <cassert>
#include "OpenGLGraphicsManager.hpp"
#include "IApplication.hpp"
#include "AssetLoader.hpp"
#include "SceneManager.hpp"
#include "IPhysicsManager.hpp"

const char VS_SHADER_SOURCE_FILE[] = "Shaders/basic.vs";
const char FS_SHADER_SOURCE_FILE[] = "Shaders/basic.fs";
#ifdef DEBUG
const char DEBUG_VS_SHADER_SOURCE_FILE[] = "Shaders/debug_vs.glsl";
const char DEBUG_PS_SHADER_SOURCE_FILE[] = "Shaders/debug_ps.glsl";
#endif

using namespace My;

static void OutputShaderErrorMessage(unsigned int shaderId,
                                     const char*  shaderFileName) {
    int           logSize, i;
    char*         infoLog;
    std::ofstream fout;

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
    std::cerr << "Error compiling shader. Check shader-error.txt for message."
              << shaderFileName << std::endl;
    return;
}

static void OutputLinkerErrorMessage(unsigned int programId) {
    int           logSize, i;
    char*         infoLog;
    std::ofstream fout;
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
    std::cerr << "Error compiling linker. Check linker-error.txt for message."
              << std::endl;
}

int OpenGLGraphicsManager::Initialize() {
    int result;
    result = GraphicsManager::Initialize();
    if (result) {
        return result;
    }
    result = gladLoadGL();
    if (!result) {
        std::cerr << "OpenGL load failed!\n";
        return -1;
    } else {
        result = 0;
        std::cout << "OpenGL Version" << glGetString(GL_VERSION) << std::endl;

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
    }

    return result;
}
void OpenGLGraphicsManager::Finalize() {
    ClearBuffers();

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

bool OpenGLGraphicsManager::SetPerFrameShaderParameters(GLuint shader) {
    unsigned int location;
    // Set world matrix
    location = glGetUniformLocation(shader, "worldMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, m_frameConstants.m_worldMatrix);

    // Set view matrix
    location = glGetUniformLocation(shader, "viewMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, m_frameConstants.m_viewMatrix);

    // Set projection matrix
    location = glGetUniformLocation(shader, "projectionMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, m_frameConstants.m_projectionMatrix);

    location = glGetUniformLocation(shader, "lightPosition");
    if (location == -1) {
        return false;
    }
    glUniform3fv(location, 1, m_frameConstants.m_lightPosition);

    location = glGetUniformLocation(shader, "lightColor");
    if (location == -1) {
        return false;
    }
    glUniform4fv(location, 1, m_frameConstants.m_lightColor);

    return true;
}

bool OpenGLGraphicsManager::SetPerBatchShaderParameters(GLuint      shader,
                                                        const char* paramName,
                                                        const mat4& param) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName);
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(GLuint      shader,
                                                        const char* paramName,
                                                        const vec3& param) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName);
    if (location == -1) {
        return false;
    }
    glUniform3fv(location, 1, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(GLuint      shader,
                                                        const char* paramName,
                                                        const float param) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName);
    if (location == -1) {
        return false;
    }
    glUniform1f(location, param);
    return true;
}
bool OpenGLGraphicsManager::SetPerBatchShaderParameters(
    GLuint shader, const char* paramName, const GLint texture_index) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName);
    if (location == -1) {
        return false;
    }

    if (texture_index < GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
        glUniform1i(location, texture_index);
    return true;
}

void OpenGLGraphicsManager::InitializeBuffers(const Scene& scene) {
    for (auto _it : scene.GeometryNodes) {
        auto pGeometryNode = _it.second;
        if (pGeometryNode->Visible()) {
            auto pGeometry =
                scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
            assert(pGeometry);
            auto pMesh = pGeometry->GetMesh().lock();
            if (!pMesh) return;

            auto vertexPropertiesCount = pMesh->GetVertexPropertiesCount();
            // auto vertexCount           = pMesh->GetVertexCount();

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
                    case VertexDataType::kFLOAT1:
                        glVertexAttribPointer(i, 1, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::kFLOAT2:
                        glVertexAttribPointer(i, 2, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::kFLOAT3:
                        glVertexAttribPointer(i, 3, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::kFLOAT4:
                        glVertexAttribPointer(i, 4, GL_FLOAT, false, 0, 0);
                        break;
                    case VertexDataType::kDOUBLE1:
                        glVertexAttribPointer(i, 1, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::kDOUBLE2:
                        glVertexAttribPointer(i, 2, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::kDOUBLE3:
                        glVertexAttribPointer(i, 3, GL_DOUBLE, false, 0, 0);
                        break;
                    case VertexDataType::kDOUBLE4:
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
                case PrimitiveType::kPOINT_LIST:
                    mode = GL_POINTS;
                    break;
                case PrimitiveType::kLINE_LIST:
                    mode = GL_LINES;
                    break;
                case PrimitiveType::kLINE_STRIP:
                    mode = GL_LINE_STRIP;
                    break;
                case PrimitiveType::kTRI_LIST:
                    mode = GL_TRIANGLES;
                    break;
                case PrimitiveType::kTRI_STRIP:
                    mode = GL_TRIANGLE_STRIP;
                    break;
                case PrimitiveType::kTRI_FAN:
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
                    case IndexDataType::kINT8:
                        type = GL_UNSIGNED_BYTE;
                        break;
                    case IndexDataType::kINT16:
                        type = GL_UNSIGNED_SHORT;
                        break;
                    case IndexDataType::kINT32:
                        type = GL_UNSIGNED_INT;
                        break;
                    default:
                        // not supported by OpenGL
                        std::cerr << "Error: Unsupported Index Type "
                                  << index_array << std::endl;
                        std::cout << "Mesh: " << *pMesh << std::endl;
                        std::cout << "Geometry: " << *pGeometry << std::endl;
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

                OpenGLDrawBatchContext& dbc = *(new OpenGLDrawBatchContext);
                dbc.vao                     = vao;
                dbc.mode                    = mode;
                dbc.type                    = type;
                dbc.node                    = pGeometryNode;
                dbc.material                = material;
                dbc.count                   = indexCount;
                m_DrawBatchContext.push_back(std::move(dbc));
            }
        }
    }
    return;
}

void OpenGLGraphicsManager::ClearBuffers() {
#ifdef DEBUG
    ClearDebugBuffers();
#endif

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
}

void OpenGLGraphicsManager::RenderBuffers() {
    glUseProgram(m_shaderProgram);
    SetPerFrameShaderParameters(m_shaderProgram);

    for (auto dbc : m_DrawBatchContext) {
        mat4 trans;

        if (void* rigidBody = dbc.node->RigidBody()) {
            // the geometry has rigid body bounded, we blend the simlation
            // result here.
            trans = g_pPhysicsManager->GetRigidBodyTransform(rigidBody);
        } else {
            trans = *dbc.node->GetCalculatedTransform();
        }

        SetPerBatchShaderParameters(m_shaderProgram, "modelMatrix", trans);
        glBindVertexArray(dbc.vao);
        // auto           indexBufferCount = dbc.counts.size();
        // const GLvoid** pIndicies        = new const
        // GLvoid*[indexBufferCount]; memset(pIndicies, 0x00,
        // sizeof(GLvoid*) * indexBufferCount);
        // glMultiDrawElements(dbc.mode, dbc.counts.data(), dbc.type,
        // pIndicies, indexBufferCount); delete[] pIndicies;

        if (dbc.material) {
            Color color = dbc.material->GetBaseColor();

            if (color.ValueMap) {
                SetPerBatchShaderParameters(
                    m_shaderProgram, "defaultSampler",
                    m_TextureIndex[color.ValueMap->GetName()]);
                SetPerBatchShaderParameters(m_shaderProgram, "diffuseColor",
                                            vec3(-1.0f));
            } else {
                SetPerBatchShaderParameters(m_shaderProgram, "diffuseColor",
                                            color.Value.rgb);
            }

            color = dbc.material->GetSpecularColor();
            SetPerBatchShaderParameters(m_shaderProgram, "specularColor",
                                        color.Value.rgb);

            Parameter param = dbc.material->GetSpecularPower();
            SetPerBatchShaderParameters(m_shaderProgram, "specularPower",
                                        param.Value);
        } else {
            SetPerBatchShaderParameters(m_shaderProgram, "diffuseColor",
                                        vec3(-1.0f));
        }

        glDrawElements(dbc.mode, dbc.count, dbc.type, 0x00);
    }

#ifdef DEBUG
    // Set the color shader as the current shader program and set the matrices
    // that it will use for rendering.
    glUseProgram(m_debugShaderProgram);

    SetPerFrameShaderParameters(m_debugShaderProgram);

    for (auto dbc : m_DebugDrawBatchContext) {
        SetPerBatchShaderParameters(m_debugShaderProgram, "lineColor",
                                    dbc.color);

        glBindVertexArray(dbc.vao);
        glDrawElements(dbc.mode, dbc.count, GL_UNSIGNED_INT, 0x00);
    }
#endif

    return;
}

bool OpenGLGraphicsManager::InitializeShaders() {
    const char* vsFilename = VS_SHADER_SOURCE_FILE;
    const char* fsFilename = FS_SHADER_SOURCE_FILE;
#ifdef DEBUG
    const char* debugVsFilename = DEBUG_VS_SHADER_SOURCE_FILE;
    const char* debugFsFilename = DEBUG_PS_SHADER_SOURCE_FILE;
#endif

    std::string vertexShaderBuffer;
    std::string fragmentShaderBuffer;
    int         status;

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

#ifdef DEBUG
    std::string debugVertexShaderBuffer;
    std::string debugFragmentShaderBuffer;

    // Load the fragment shader source file into a text buffer.
    debugVertexShaderBuffer =
        g_pAssetLoader->SyncOpenAndReadTextFileToString(debugVsFilename);
    if (debugVertexShaderBuffer.empty()) {
        return false;
    }

    // Load the fragment shader source file into a text buffer.
    debugFragmentShaderBuffer =
        g_pAssetLoader->SyncOpenAndReadTextFileToString(debugFsFilename);
    if (debugFragmentShaderBuffer.empty()) {
        return false;
    }
#endif

    m_vertexShader   = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

#ifdef DEBUG
    m_debugVertexShader   = glCreateShader(GL_VERTEX_SHADER);
    m_debugFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
#endif

    const char* _v_c_str = vertexShaderBuffer.c_str();
    const char* _f_c_str = fragmentShaderBuffer.c_str();
    glShaderSource(m_vertexShader, 1, &_v_c_str, NULL);
    glShaderSource(m_fragmentShader, 1, &_f_c_str, NULL);

#ifdef DEBUG
    const char* _v_c_str_debug = debugVertexShaderBuffer.c_str();
    glShaderSource(m_debugVertexShader, 1, &_v_c_str_debug, NULL);
    const char* _f_c_str_debug = debugFragmentShaderBuffer.c_str();
    glShaderSource(m_debugFragmentShader, 1, &_f_c_str_debug, NULL);
#endif

    glCompileShader(m_vertexShader);
    glCompileShader(m_fragmentShader);

#ifdef DEBUG
    glCompileShader(m_debugVertexShader);
    glCompileShader(m_debugFragmentShader);
#endif

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

#ifdef DEBUG
    glGetShaderiv(m_debugVertexShader, GL_COMPILE_STATUS, &status);
    if (status != 1) {
        OutputShaderErrorMessage(m_debugVertexShader, debugVsFilename);
        return false;
    }
    glGetShaderiv(m_debugFragmentShader, GL_COMPILE_STATUS, &status);
    if (status != 1) {
        OutputShaderErrorMessage(m_debugFragmentShader, debugFsFilename);
        return false;
    }
#endif

    m_shaderProgram = glCreateProgram();
#ifdef DEBUG
    m_debugShaderProgram = glCreateProgram();
#endif

    glAttachShader(m_shaderProgram, m_vertexShader);
    glAttachShader(m_shaderProgram, m_fragmentShader);
#ifdef DEBUG
    glAttachShader(m_debugShaderProgram, m_debugVertexShader);
    glAttachShader(m_debugShaderProgram, m_debugFragmentShader);
#endif

    glBindAttribLocation(m_shaderProgram, 0, "inputPosition");
    glBindAttribLocation(m_shaderProgram, 1, "inputNormal");
    glBindAttribLocation(m_shaderProgram, 2, "inputUV");

    glLinkProgram(m_shaderProgram);

#ifdef DEBUG
    glBindAttribLocation(m_debugShaderProgram, 0, "inputPosition");
    glLinkProgram(m_debugShaderProgram);
#endif

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &status);
    if (status != 1) {
        OutputLinkerErrorMessage(m_shaderProgram);
        return false;
    }

#ifdef DEBUG
    glGetProgramiv(m_debugShaderProgram, GL_LINK_STATUS, &status);
    if (status != 1) {
        OutputLinkerErrorMessage(m_debugShaderProgram);
        return false;
    }
#endif

    return true;
}

#ifdef DEBUG
void OpenGLGraphicsManager::DrawLine(const vec3& from, const vec3& to,
                                     const vec3& color) {
    GLfloat vertices[6];
    vertices[0] = from.x;
    vertices[1] = from.y;
    vertices[2] = from.z;
    vertices[3] = to.x;
    vertices[4] = to.y;
    vertices[5] = to.z;

    GLuint index[] = {0, 1};

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    m_DebugBuffers.push_back(buffer_id);

    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    m_DebugBuffers.push_back(buffer_id);

    DebugDrawBatchContext& dbc = *(new DebugDrawBatchContext);

    dbc.vao   = vao;
    dbc.mode  = GL_LINES;
    dbc.count = 2;
    dbc.color = color;

    m_DebugDrawBatchContext.push_back(std::move(dbc));
}

void OpenGLGraphicsManager::DrawBox(const vec3& bbMin, const vec3& bbMax,
                                    const vec3& color) {
    GLfloat vertices[24];
    // top
    vertices[0] = bbMax.x;
    vertices[1] = bbMax.y;
    vertices[2] = bbMax.z;

    vertices[3] = bbMax.x;
    vertices[4] = bbMin.y;
    vertices[5] = bbMax.z;

    vertices[6] = bbMin.x;
    vertices[7] = bbMin.y;
    vertices[8] = bbMax.z;

    vertices[9]  = bbMin.x;
    vertices[10] = bbMax.y;
    vertices[11] = bbMax.z;

    // bottom
    vertices[12] = bbMax.x;
    vertices[13] = bbMax.y;
    vertices[14] = bbMin.z;

    vertices[15] = bbMax.x;
    vertices[16] = bbMin.y;
    vertices[17] = bbMin.z;

    vertices[18] = bbMin.x;
    vertices[19] = bbMin.y;
    vertices[20] = bbMin.z;

    vertices[21] = bbMin.x;
    vertices[22] = bbMax.y;
    vertices[23] = bbMin.z;

    GLuint index[] = {0, 1, 2, 3, 0, 4, 5, 1, 5, 6, 2, 6, 7, 3, 7, 4};

    GLuint vao, buffer_id;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    m_DebugBuffers.push_back(buffer_id);

    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    m_DebugBuffers.push_back(buffer_id);

    DebugDrawBatchContext& dbc = *(new DebugDrawBatchContext);
    dbc.vao                    = vao;
    dbc.mode                   = GL_LINE_STRIP;
    dbc.count                  = 16;
    dbc.color                  = color;

    m_DebugDrawBatchContext.push_back(std::move(dbc));
}

void OpenGLGraphicsManager::ClearDebugBuffers() {
    for (auto dbc : m_DebugDrawBatchContext) {
        glDeleteVertexArrays(1, &dbc.vao);
    }

    m_DebugDrawBatchContext.clear();

    for (auto buf : m_DebugBuffers) {
        glDeleteBuffers(1, &buf);
    }
    m_DebugBuffers.clear();
}
#endif
