#include "OpenGLGraphicsManager.hpp"

#include <fstream>

#include "GLFWApplication.hpp"
#include "IPhysicsManager.hpp"

namespace Hitagi::Graphics::OpenGL {

int OpenGLGraphicsManager::Initialize() {
    int         result;
    GLFWwindow* window = static_cast<GLFWApplication*>(g_App.get())->GetWindow();
    glfwSetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwSetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwSetWindowAttrib(window, GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwSetWindowAttrib(window, GLFW_SAMPLES, 4);
    glfwMakeContextCurrent(window);

    result = gladLoadGL();
    if (!result) {
        std::cerr << "OpenGL load failed!\n";
        return -1;
    } else {
        std::cout << "OpenGL Version" << glGetString(GL_VERSION) << std::endl;

        if (GLAD_GL_VERSION_3_0) {
            // Set the depth buffer to be entirely cleared to 1.0 values.
            glClearDepth(1.0f);

            // Enable depth testing.
            glEnable(GL_DEPTH_TEST);

            // Enable multisample
            glEnable(GL_MULTISAMPLE);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Set the polygon winding to front facing for the left handed
            // system.
            glFrontFace(GL_CCW);

            // Enable back face culling.
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }
    }
    glfwSwapInterval(1);
    result = GraphicsManager::Initialize();
    return result;
}
void OpenGLGraphicsManager::Tick() { GraphicsManager::Tick(); }
void OpenGLGraphicsManager::Finalize() {
    ClearBuffers();
    ClearShaders();

    GraphicsManager::Finalize();
}

void OpenGLGraphicsManager::Clear() {
    GraphicsManager::Clear();
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsManager::Draw() {
    GraphicsManager::Draw();
    GLFWwindow* window = static_cast<GLFWApplication*>(g_App.get())->GetWindow();
    glfwSwapBuffers(window);
    glFlush();
}

bool OpenGLGraphicsManager::UpdateFrameParameters(GLuint shader) {
    SetShaderParameters(shader, "worldMatrix", mat4f(1.0f));
    SetShaderParameters(shader, "viewMatrix", m_FrameConstants.view);
    SetShaderParameters(shader, "projectionMatrix", m_FrameConstants.projection);
    SetShaderParameters(shader, "lightPosition", m_FrameConstants.lightPosition);
    SetShaderParameters(shader, "lightColor", m_FrameConstants.lightIntensity);
    return true;
}
bool OpenGLGraphicsManager::SetShaderParameters(GLuint shader, const char* paramName, TshaderParameter param) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName);
    if (location == -1) {
        std::cerr << "[OpenGL] Set shader parameter failed. parmeter: " << paramName << std::endl;
        return false;
    }
    if (std::holds_alternative<float>(param))
        glUniform1f(location, std::get<float>(param));
    else if (std::holds_alternative<vec2f>(param))
        glUniform2fv(location, 1, std::get<vec2f>(param));
    else if (std::holds_alternative<vec3f>(param))
        glUniform3fv(location, 1, std::get<vec3f>(param));
    else if (std::holds_alternative<vec4f>(param))
        glUniform4fv(location, 1, std::get<vec4f>(param));
    else if (std::holds_alternative<mat4f>(param))
        glUniformMatrix4fv(location, 1, true, std::get<mat4f>(param));
    else if (std::holds_alternative<GLuint>(param))
        if (std::get<GLuint>(param) < GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
            glUniform1i(location, std::get<GLuint>(param));
    return true;
}

void OpenGLGraphicsManager::InitializeBuffers(const Resource::Scene& scene) {
    // Initialize Mesh
    for (auto&& [key, geometry] : scene.Geometries) {
        for (auto&& mesh : geometry->GetMeshes()) {
            auto mb = std::make_shared<MeshBuffer>();
            glGenVertexArrays(1, &mb->vao);
            glBindVertexArray(mb->vao);

            // Vertex Buffer
            for (size_t i = 0; i < m_BasicShader.layout.size(); i++) {
                if (!mesh->HasProperty(m_BasicShader.layout[i])) continue;
                auto&  vertexArray    = mesh->GetVertexPropertyArray(m_BasicShader.layout[i]);
                auto   vertexData     = vertexArray.GetData();
                auto   vertexDataSize = vertexArray.GetDataSize();
                GLuint vbo;
                glGenBuffers(1, &vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, vertexDataSize, vertexData, GL_STATIC_DRAW);
                glEnableVertexAttribArray(i);
                switch (vertexArray.GetDataType()) {
                    case Resource::VertexDataType::FLOAT1:
                        glVertexAttribPointer(i, 1, GL_FLOAT, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::FLOAT2:
                        glVertexAttribPointer(i, 2, GL_FLOAT, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::FLOAT3:
                        glVertexAttribPointer(i, 3, GL_FLOAT, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::FLOAT4:
                        glVertexAttribPointer(i, 4, GL_FLOAT, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::DOUBLE1:
                        glVertexAttribPointer(i, 1, GL_DOUBLE, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::DOUBLE2:
                        glVertexAttribPointer(i, 2, GL_DOUBLE, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::DOUBLE3:
                        glVertexAttribPointer(i, 3, GL_DOUBLE, false, 0, nullptr);
                        break;
                    case Resource::VertexDataType::DOUBLE4:
                        glVertexAttribPointer(i, 4, GL_DOUBLE, false, 0, nullptr);
                        break;
                    default:
                        assert(0);
                        break;
                }
                mb->vbos.push_back(vbo);
            }
            // Index Array
            glGenBuffers(1, &mb->ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mb->ebo);
            auto& indexArray         = mesh->GetIndexArray();
            auto  indexArrayData     = indexArray.GetData();
            auto  indexArrayDataSize = indexArray.GetDataSize();
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexArrayDataSize, indexArrayData, GL_STATIC_DRAW);
            mb->indexCount = indexArray.GetIndexCount();
            switch (indexArray.GetIndexType()) {
                case Resource::IndexDataType::INT8:
                    mb->type = GL_UNSIGNED_BYTE;
                    break;
                case Resource::IndexDataType::INT16:
                    mb->type = GL_UNSIGNED_SHORT;
                    break;
                case Resource::IndexDataType::INT32:
                    mb->type = GL_UNSIGNED_INT;
                    break;
                default:
                    // not supported by OpenGL
                    std::cerr << "Error: Unsupported Index Type " << indexArray << std::endl;
                    std::cout << "Mesh: " << *mesh << std::endl;
                    continue;
            }
            // Primitive
            switch (mesh->GetPrimitiveType()) {
                case Resource::PrimitiveType::POINT_LIST:
                    mb->mode = GL_POINTS;
                    break;
                case Resource::PrimitiveType::LINE_LIST:
                    mb->mode = GL_LINES;
                    break;
                case Resource::PrimitiveType::LINE_STRIP:
                    mb->mode = GL_LINE_STRIP;
                    break;
                case Resource::PrimitiveType::TRI_LIST:
                    mb->mode = GL_TRIANGLES;
                    break;
                case Resource::PrimitiveType::TRI_STRIP:
                    mb->mode = GL_TRIANGLE_STRIP;
                    break;
                case Resource::PrimitiveType::TRI_FAN:
                    mb->mode = GL_TRIANGLE_FAN;
                    break;
                default:
                    continue;
            }
            mb->material                   = mesh->GetMaterial();
            m_MeshBuffers[mesh->GetGuid()] = mb;
        }
    }

    // Initialize Texture
    for (auto&& [key, material] : scene.Materials) {
        auto color = material->GetDiffuseColor();
        if (color.ValueMap) {
            auto& guid  = color.ValueMap->GetGuid();
            auto& image = color.ValueMap->GetTextureImage();
            auto  it    = m_Textures.find(guid);
            if (it == m_Textures.end()) {
                GLuint textureId;
                glGenTextures(1, &textureId);
                glActiveTexture(GL_TEXTURE0 + textureId);
                glBindTexture(GL_TEXTURE_2D, textureId);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.GetWidth(), image.GetHeight(), 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, image.getData());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                m_Textures[guid] = textureId;
            }
        }
    }

    for (auto [key, node] : scene.GeometryNodes) {
        if (auto geometry = node->GetSceneObjectRef().lock()) {
            for (auto&& mesh : geometry->GetMeshes()) {
                m_DrawBatchContext.push_back({node, m_MeshBuffers[mesh->GetGuid()]});
            }
        }
    }

    // Initialize text buffer
    glGenVertexArrays(1, &m_TextRenderVAO);
    glGenBuffers(1, &m_TextRenderVBO);
    glBindVertexArray(m_TextRenderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_TextRenderVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool OpenGLGraphicsManager::InitializeShaders() {
    auto OutputShaderErrorMessage = [](GLuint id, const std::filesystem::path& shaderFileName) -> void {
        int           logSize;
        char*         infoLog;
        std::ofstream fout("shader-error.txt");
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logSize);
        infoLog = new char[logSize];
        glGetShaderInfoLog(id, logSize, nullptr, infoLog);
        for (int i = 0; i < logSize; i++) {
            fout << infoLog[i];
        }
        fout.close();
        std::cerr << "[OpenGL] Error compiling shader. Check shader-error.txt for message." << shaderFileName
                  << std::endl;
        delete[] infoLog;
    };

    auto compileShader = [&](ShaderProgram& program) {
        int status;
        program.programId = glCreateProgram();
        for (auto&& shaderInfo : program.shaders) {
            auto        shaderBuffer = g_FileIOManager->SyncOpenAndReadBinary(shaderInfo.path);
            char*       c_str        = reinterpret_cast<char*>(shaderBuffer.GetData());
            const GLint shaderSeize  = shaderBuffer.GetDataSize();
            shaderInfo.shaderId      = glCreateShader(shaderInfo.type);
            glShaderSource(shaderInfo.shaderId, 1, &c_str, &shaderSeize);
            glCompileShader(shaderInfo.shaderId);
            glGetShaderiv(shaderInfo.shaderId, GL_COMPILE_STATUS, &status);
            if (status != 1) OutputShaderErrorMessage(shaderInfo.shaderId, shaderInfo.path);
            glAttachShader(program.programId, shaderInfo.shaderId);
        }
        for (size_t i = 0; i < program.layout.size(); i++)
            glBindAttribLocation(program.programId, i, program.layout[i].c_str());
        glLinkProgram(program.programId);
        glGetProgramiv(program.programId, GL_LINK_STATUS, &status);
        if (status != 1) OutputShaderErrorMessage(program.programId, "");
    };

    compileShader(m_BasicShader);
    compileShader(m_TextShader);
    int status;
#if defined(_DEBUG)
    compileShader(m_DebugShader);
#endif
    return true;
}

void OpenGLGraphicsManager::ClearBuffers() {
#if defined(_DEBUG)
    ClearDebugBuffers();
#endif
    m_DrawBatchContext.clear();
    for (auto&& [guid, mesh] : m_MeshBuffers) {
        glDeleteVertexArrays(1, &mesh->vao);
        glDeleteBuffers(1, &mesh->ebo);
        for (auto&& vbo : mesh->vbos) glDeleteBuffers(1, &vbo);
    }
    m_MeshBuffers.clear();

    for (auto [guid, texture] : m_Textures) {
        glDeleteTextures(1, &texture);
    }
    m_Textures.clear();

    for (auto&& [c, ci] : m_Characters) {
        glDeleteTextures(1, &ci.textureID);
    }
    m_Characters.clear();
    glDeleteVertexArrays(1, &m_TextRenderVAO);
    glDeleteBuffers(1, &m_TextRenderVBO);
}

void OpenGLGraphicsManager::ClearShaders() {
    if (m_BasicShader.programId) {
        for (auto&& shaderInfo : m_BasicShader.shaders) {
            glDeleteShader(shaderInfo.shaderId);
        }
        glDeleteProgram(m_BasicShader.programId);
    }
    if (m_TextShader.programId) {
        for (auto&& shaderInfo : m_TextShader.shaders) {
            glDeleteShader(shaderInfo.shaderId);
        }
        glDeleteProgram(m_TextShader.programId);
    }

#if defined(_DEBUG)
    if (m_DebugShader.programId) {
        for (auto&& shaderInfo : m_DebugShader.shaders) {
            glDeleteShader(shaderInfo.shaderId);
        }
        glDeleteProgram(m_DebugShader.programId);
    }
#endif  // DEBUG
}

void OpenGLGraphicsManager::RenderBuffers() {
    glUseProgram(m_BasicShader.programId);
    UpdateFrameParameters(m_BasicShader.programId);

    for (auto dbc : m_DrawBatchContext) {
        auto node = dbc.node.lock();
        if (!node && !node->Visible()) continue;
        mat4f trans;
        trans = node->GetCalculatedTransform();

        SetShaderParameters(m_BasicShader.programId, "modelMatrix", trans);
        glBindVertexArray(dbc.mesh->vao);

        if (auto material = dbc.mesh->material.lock()) {
            Resource::Color color = material->GetDiffuseColor();

            if (color.ValueMap) {
                SetShaderParameters(m_BasicShader.programId, "defaultSampler", m_Textures[color.ValueMap->GetGuid()]);
                SetShaderParameters(m_BasicShader.programId, "diffuseColor", vec3f(-1.0f));
            } else {
                SetShaderParameters(m_BasicShader.programId, "diffuseColor", color.Value.rgb);
            }

            color = material->GetSpecularColor();
            SetShaderParameters(m_BasicShader.programId, "specularColor", color.Value.rgb);

            Resource::Parameter param = material->GetSpecularPower();
            SetShaderParameters(m_BasicShader.programId, "specularPower", param.Value);
        } else {
            SetShaderParameters(m_BasicShader.programId, "diffuseColor", vec3f(-1.0f));
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dbc.mesh->ebo);
        glDrawElements(dbc.mesh->mode, dbc.mesh->indexCount, dbc.mesh->type, nullptr);
    }

    // Render text
    glUseProgram(m_TextShader.programId);
    SetShaderParameters(m_TextShader.programId, "projection", ortho(0.0f, 1024.0f, 0.0f, 720.0f, 0.0f, 100.0f));
    while (!m_textRenderQueue.empty()) {
        auto& textInfo = m_textRenderQueue.front();
        SetShaderParameters(m_TextShader.programId, "textColor", textInfo.color);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(m_TextRenderVAO);
        for (auto&& c : textInfo.text) {
            GLfloat posX   = textInfo.position.x + m_Characters[c].bearing.x;
            GLfloat posY   = textInfo.position.y - (m_Characters[c].size.y - m_Characters[c].bearing.y);
            GLfloat width  = m_Characters[c].size.x;
            GLfloat height = m_Characters[c].size.y;
            // clang-format off
            std::array vertices {
                // position                   texcoord
                posX        , posY + height, 0.0f, 0.0f,
                posX        , posY         , 0.0f, 1.0f,
                posX + width, posY         , 1.0f, 1.0f,

                posX        , posY + height, 0.0f, 0.0f,
                posX + width, posY         , 1.0f, 1.0f,
                posX + width, posY + height, 1.0f, 0.0f,
            };
            // clang-format on
            glBindTexture(GL_TEXTURE_2D, m_Characters[c].textureID);
            glBindBuffer(GL_ARRAY_BUFFER, m_TextRenderVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            textInfo.position.x += (m_Characters[c].advance >> 6);
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_textRenderQueue.pop();
    }

#if defined(_DEBUG)
    if (!m_DebugDrawBatchContext.empty()) {
        // Set the color shader as the current shader program and set the matrices
        // that it will use for rendering.
        glUseProgram(m_DebugShader.programId);
        SetShaderParameters(m_DebugShader.programId, "WVP", m_FrameConstants.projView);
        for (auto dbc : m_DebugDrawBatchContext) {
            SetShaderParameters(m_DebugShader.programId, "lineColor", dbc.color);

            glBindVertexArray(dbc.vao);
            glDrawElements(dbc.mode, dbc.count, GL_UNSIGNED_INT, 0x00);
        }
    }
#endif
}

void OpenGLGraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (auto&& c : text) {
        if (m_Characters.find(c) == m_Characters.end()) {
            auto   glyph = GetGlyph(c);
            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, glyph->bitmap.pitch, glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                         glyph->bitmap.buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            CharacterInfo ci;
            ci.textureID = texture;
            ci.bearing   = {glyph->bitmap_left, glyph->bitmap_top};
            ci.size      = {glyph->bitmap.width, glyph->bitmap.rows};
            ci.advance   = glyph->advance.x;
            m_Characters.insert({c, ci});
        }
    }
    m_textRenderQueue.push({std::string(text), position, color});
}

#if defined(_DEBUG)
// void OpenGLGraphicsManager::RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) {
//     GLfloat vertices[6];
//     vertices[0] = from.x;
//     vertices[1] = from.y;
//     vertices[2] = from.z;
//     vertices[3] = to.x;
//     vertices[4] = to.y;
//     vertices[5] = to.z;

//     GLuint index[] = {0, 1};

//     GLuint vao;
//     glGenVertexArrays(1, &vao);
//     glBindVertexArray(vao);

//     GLuint buffer_id;
//     glGenBuffers(1, &buffer_id);
//     glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//     m_DebugBuffers.push_back(buffer_id);

//     glGenBuffers(1, &buffer_id);
//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
//     glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);

//     glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
//     glEnableVertexAttribArray(0);
//     m_DebugBuffers.push_back(buffer_id);

//     DebugDrawBatchContext dbc;

//     dbc.vao   = vao;
//     dbc.mode  = GL_LINES;
//     dbc.count = 2;
//     dbc.color = color;

//     m_DebugDrawBatchContext.push_back(std::move(dbc));
// }

// void OpenGLGraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) {
//     GLfloat vertices[24];
//     // top
//     vertices[0] = bbMax.x;
//     vertices[1] = bbMax.y;
//     vertices[2] = bbMax.z;

//     vertices[3] = bbMax.x;
//     vertices[4] = bbMin.y;
//     vertices[5] = bbMax.z;

//     vertices[6] = bbMin.x;
//     vertices[7] = bbMin.y;
//     vertices[8] = bbMax.z;

//     vertices[9]  = bbMin.x;
//     vertices[10] = bbMax.y;
//     vertices[11] = bbMax.z;

//     // bottom
//     vertices[12] = bbMax.x;
//     vertices[13] = bbMax.y;
//     vertices[14] = bbMin.z;

//     vertices[15] = bbMax.x;
//     vertices[16] = bbMin.y;
//     vertices[17] = bbMin.z;

//     vertices[18] = bbMin.x;
//     vertices[19] = bbMin.y;
//     vertices[20] = bbMin.z;

//     vertices[21] = bbMin.x;
//     vertices[22] = bbMax.y;
//     vertices[23] = bbMin.z;

//     GLuint index[] = {0, 1, 2, 3, 0, 4, 5, 1, 5, 6, 2, 6, 7, 3, 7, 4};

//     GLuint vao, buffer_id;
//     glGenVertexArrays(1, &vao);
//     glBindVertexArray(vao);

//     glGenBuffers(1, &buffer_id);
//     glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//     m_DebugBuffers.push_back(buffer_id);

//     glGenBuffers(1, &buffer_id);
//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
//     glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);

//     glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
//     glEnableVertexAttribArray(0);
//     m_DebugBuffers.push_back(buffer_id);

//     DebugDrawBatchContext dbc;
//     dbc.vao   = vao;
//     dbc.mode  = GL_LINE_STRIP;
//     dbc.count = 16;
//     dbc.color = color;

//     m_DebugDrawBatchContext.push_back(std::move(dbc));
// }

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
}  // namespace Hitagi::Graphics::OpenGL