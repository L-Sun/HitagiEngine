#include "OpenGLGraphicsManager.hpp"

#include <fstream>

#include "GLFWApplication.hpp"
#include "IPhysicsManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi::Graphics::OpenGL {

int OpenGLGraphicsManager::Initialize() {
    int result = GraphicsManager::Initialize();
    if (result != 0) return result;
    GLFWwindow* window = static_cast<GLFWApplication*>(g_App.get())->GetWindow();
    glfwSetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwSetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwSetWindowAttrib(window, GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwSetWindowAttrib(window, GLFW_SAMPLES, 4);
    glfwMakeContextCurrent(window);

    if (!gladLoadGL()) {
        m_Logger->error("OpenGL load failed!");
        return -1;
    } else {
        m_Logger->info("OpenGL Version: {}", glGetString(GL_VERSION));

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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsManager::Draw() {
    GraphicsManager::Draw();
    GLFWwindow* window = static_cast<GLFWApplication*>(g_App.get())->GetWindow();
    glfwSwapBuffers(window);
    glFlush();
}

bool OpenGLGraphicsManager::UpdateFrameParameters(GLuint shader) {
    SetShaderParameters(shader, "viewMatrix", m_FrameConstants.view);
    SetShaderParameters(shader, "projectionMatrix", m_FrameConstants.projection);
    SetShaderParameters(shader, "lightPosInView", m_FrameConstants.lightPosInView);
    SetShaderParameters(shader, "lightColor", m_FrameConstants.lightIntensity);
    return true;
}
bool OpenGLGraphicsManager::SetShaderParameters(GLuint shader, std::string_view paramName, TshaderParameter param) {
    unsigned int location;
    location = glGetUniformLocation(shader, paramName.data());
    if (location == -1) {
        m_Logger->info("Set shader parameter failed. parmeter: {}", paramName);
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

int OpenGLGraphicsManager::GetGLPrimitiveType(Resource::PrimitiveType type) {
    switch (type) {
        case Resource::PrimitiveType::POINT_LIST:
            return GL_POINTS;
        case Resource::PrimitiveType::LINE_LIST:
            return GL_LINES;
        case Resource::PrimitiveType::LINE_STRIP:
            return GL_LINE_STRIP;
        case Resource::PrimitiveType::LINE_LIST_ADJACENCY:
            return GL_LINES_ADJACENCY;
        case Resource::PrimitiveType::LINE_LOOP:
            return GL_LINE_LOOP;
        case Resource::PrimitiveType::TRI_LIST:
            return GL_TRIANGLES;
        case Resource::PrimitiveType::TRI_STRIP:
            return GL_TRIANGLE_STRIP;
        case Resource::PrimitiveType::TRI_FAN:
            return GL_TRIANGLE_FAN;
        case Resource::PrimitiveType::TRI_LIST_ADJACENCY:
            return GL_TRIANGLES_ADJACENCY;
        case Resource::PrimitiveType::TRI_STRIP_ADJACENCY:
            return GL_TRIANGLE_STRIP_ADJACENCY;
        case Resource::PrimitiveType::POLYGON:
            return GL_POLYGON;
        case Resource::PrimitiveType::QUAD_LIST:
            return GL_QUADS;
        case Resource::PrimitiveType::QUAD_STRIP:
            return GL_QUAD_STRIP;
        default:
            return GL_POINTS;
    }
}

std::shared_ptr<OpenGLGraphicsManager::MeshBuffer> OpenGLGraphicsManager::CreateMeshBuffer(const Resource::SceneObjectMesh& mesh) {
    auto mb = std::make_shared<MeshBuffer>();
    glGenVertexArrays(1, &mb->vao);
    glBindVertexArray(mb->vao);

    // Vertex Buffer
    for (size_t i = 0; i < m_BasicShader.layout.size(); i++) {
        if (!mesh.HasProperty(m_BasicShader.layout[i])) continue;
        auto&  vertexArray    = mesh.GetVertexPropertyArray(m_BasicShader.layout[i]);
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
    auto& indexArray         = mesh.GetIndexArray();
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
        default: {
            // not supported by OpenGL
            m_Logger->error("Unsupported Index Type");
            m_Logger->error("Mesh", mesh);
            glDeleteVertexArrays(1, &mb->vao);
            glDeleteBuffers(1, &mb->ebo);
            for (auto&& vbo : mb->vbos) glDeleteBuffers(1, &vbo);
            return nullptr;
        }
    }
    // Primitive
    mb->mode     = GetGLPrimitiveType(mesh.GetPrimitiveType());
    mb->material = mesh.GetMaterial();
    return mb;
}

void OpenGLGraphicsManager::InitializeBuffers(const Resource::Scene& scene) {
    // Initialize Mesh
    for (auto&& [key, geometry] : scene.Geometries)
        for (auto&& mesh : geometry->GetMeshes())
            if (auto mb = CreateMeshBuffer(*mesh); mb != nullptr)
                m_MeshBuffers[mesh->GetGuid()] = std::move(mb);

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
                             GL_UNSIGNED_BYTE, image.GetData());
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
    enum ShaderErrorType {
        COMPILE_ERROR,
        LINK_ERROR,
    };
    auto OutputShaderErrorMessage = [logger = m_Logger](GLuint id, const std::filesystem::path& shaderFileName, ShaderErrorType errorType) -> void {
        int   logSize;
        char* infoLog;
        if (errorType == COMPILE_ERROR) {
            logger->error("Compiling shader failed. {}", shaderFileName);
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logSize);
            infoLog = new char[logSize];
            glGetShaderInfoLog(id, logSize, nullptr, infoLog);
        } else if (errorType == LINK_ERROR) {
            logger->error("Link shader failed.");
            glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logSize);
            infoLog = new char[logSize];
            glGetProgramInfoLog(id, logSize, nullptr, infoLog);
        }

        std::stringstream ss{infoLog};
        std::string       singleLine;
        while (ss >> std::ws) {
            std::getline(ss, singleLine);
            if (!singleLine.empty()) logger->error(singleLine);
            singleLine.clear();
        }
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
            if (status != 1) OutputShaderErrorMessage(shaderInfo.shaderId, shaderInfo.path, COMPILE_ERROR);
            glAttachShader(program.programId, shaderInfo.shaderId);
        }
        for (size_t i = 0; i < program.layout.size(); i++)
            glBindAttribLocation(program.programId, i, program.layout[i].c_str());
        glLinkProgram(program.programId);
        glGetProgramiv(program.programId, GL_LINK_STATUS, &status);
        if (status != 1) OutputShaderErrorMessage(program.programId, "", LINK_ERROR);
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
        SetShaderParameters(m_DebugShader.programId, "projView", m_FrameConstants.projView);
        for (auto dbc : m_DebugDrawBatchContext) {
            SetShaderParameters(m_DebugShader.programId, "color", dbc.color);

            glBindVertexArray(dbc.mesh->vao);
            glDrawElements(dbc.mesh->mode, dbc.mesh->indexCount, dbc.mesh->type, nullptr);
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
            glActiveTexture(GL_TEXTURE0 + texture);
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
void OpenGLGraphicsManager::DrawDebugMesh(const Resource::SceneObjectMesh& mesh, const mat4f& transform, const vec4f& color) {
    DebugDrawBatchContext dbc;
    dbc.mesh  = CreateMeshBuffer(mesh);
    dbc.color = color;
    dbc.node  = Resource::SceneGeometryNode();
    dbc.node.ApplyTransform(transform);
    m_DebugDrawBatchContext.emplace_back(std::move(dbc));
}

void OpenGLGraphicsManager::ClearDebugBuffers() {
    for (auto dbc : m_DebugDrawBatchContext) {
        glDeleteVertexArrays(1, &dbc.mesh->vao);
        glDeleteBuffers(1, &dbc.mesh->ebo);
        for (auto&& vbo : dbc.mesh->vbos) glDeleteBuffers(1, &vbo);
    }

    m_DebugDrawBatchContext.clear();
}
#endif
}  // namespace Hitagi::Graphics::OpenGL