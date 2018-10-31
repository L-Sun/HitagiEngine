#include <iostream>
#include <fstream>
#include "OpenGLGraphicsManager.hpp"
#include "IApplication.hpp"
#include "AssetLoader.hpp"
#include "SceneManager.hpp"

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

            m_DrawFrameContext.m_worldMatrix = glm::mat4(1.0f);
        }
        InitializeShader(VS_SHADER_SOURCE_FILE, FS_SHADER_SOURCE_FILE);
        InitializeBuffers();
    }

    return result;
}
void OpenGLGraphicsManager::Finalize() {
    for (size_t i = 0; i < m_Buffers.size(); i++) {
        glDisableVertexAttribArray(i);
    }
    for (auto buf : m_Buffers) {
        if (buf.first == "index") {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glDeleteBuffers(1, &buf.second);
    }
    glDetachShader(m_shaderProgram, m_vertexShader);
    glDetachShader(m_shaderProgram, m_fragmentShader);
    glDeleteShader(m_vertexShader);
    glDeleteShader(m_fragmentShader);
    glDeleteProgram(m_shaderProgram);
}

void OpenGLGraphicsManager::Tick() {}

void OpenGLGraphicsManager::Clear() {
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsManager::Draw() {
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
    glUniformMatrix4fv(location, 1, false,
                       &m_DrawFrameContext.m_worldMatrix[0][0]);

    // Set view matrix
    location = glGetUniformLocation(m_shaderProgram, "viewMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false,
                       &m_DrawFrameContext.m_viewMatrix[0][0]);

    // Set projection matrix
    location = glGetUniformLocation(m_shaderProgram, "projectionMatrix");
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false,
                       &m_DrawFrameContext.m_projectionMatrix[0][0]);

    location = glGetUniformLocation(m_shaderProgram, "lightPosition");
    if (location == -1) {
        return false;
    }
    glUniform3fv(location, 1, &m_DrawFrameContext.m_lightPosition[0]);

    location = glGetUniformLocation(m_shaderProgram, "lightColor");
    if (location == -1) {
        return false;
    }
    glUniform4fv(location, 1, &m_DrawFrameContext.m_lightColor[0]);

    return true;
}

bool OpenGLGraphicsManager::SetPerBatchShaderParameters(const char* paramName,
                                                        float*      param) {
    unsigned int location;
    location = glGetUniformLocation(m_shaderProgram, paramName);
    if (location == -1) {
        return false;
    }
    glUniformMatrix4fv(location, 1, false, param);
    return true;
}

void OpenGLGraphicsManager::InitializeBuffers() {
    auto& scene         = g_pSceneManager->GetSceneForRendering();
    auto  pGeometryNode = scene.GetFirstGeometryNode();

    while (pGeometryNode) {
        auto pGeometry = scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
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
            auto v_property_array_data_size = v_property_array.GetDataSize();
            auto v_property_array_data      = v_property_array.GetData();

            glGenBuffers(1, &buffer_id);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
            glBufferData(GL_ARRAY_BUFFER, v_property_array_data_size,
                         v_property_array_data, GL_STATIC_DRAW);

            glEnableVertexAttribArray(i);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_id);

            switch (v_property_array.GetDataType()) {
                case VertexDataType::VERTEX_DATA_TYPE_FLOAT1:
                    glVertexAttribPointer(i, 1, GL_FLOAT, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_FLOAT2:
                    glVertexAttribPointer(i, 2, GL_FLOAT, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_FLOAT3:
                    glVertexAttribPointer(i, 3, GL_FLOAT, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_FLOAT4:
                    glVertexAttribPointer(i, 4, GL_FLOAT, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_DOUBLE1:
                    glVertexAttribPointer(i, 1, GL_DOUBLE, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_DOUBLE2:
                    glVertexAttribPointer(i, 2, GL_DOUBLE, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_DOUBLE3:
                    glVertexAttribPointer(i, 3, GL_DOUBLE, false, 0, 0);
                    break;
                case VertexDataType::VERTEX_DATA_TYPE_DOUBLE4:
                    glVertexAttribPointer(i, 4, GL_DOUBLE, false, 0, 0);
                    break;
                default:
                    assert(0);
                    break;
            }
            m_Buffers[v_property_array.GetAttributeName()] = buffer_id;
        }

        glGenBuffers(1, &buffer_id);
        const SceneObjectIndexArray& index_array = pMesh->GetIndexArray(0);
        auto index_array_size                    = index_array.GetDataSize();
        auto index_array_data                    = index_array.GetData();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array_size,
                     index_array_data, GL_STATIC_DRAW);

        GLsizei indexCount = static_cast<GLsizei>(index_array.GetIndexCount());
        GLenum  mode;

        switch (pMesh->GetPrimitiveType()) {
            case PrimitiveType::PRIMITIVE_TYPE_POINT_LIST:
                mode = GL_POINTS;
                break;
            case PrimitiveType::PRIMITIVE_TYPE_LINE_LIST:
                mode = GL_LINES;
                break;
            case PrimitiveType::PRIMITIVE_TYPE_LINE_STRIP:
                mode = GL_LINE_STRIP;
                break;
            case PrimitiveType::PRIMITIVE_TYPE_TRI_LIST:
                mode = GL_TRIANGLES;
                break;
            case PrimitiveType::PRIMITIVE_TYPE_TRI_STRIP:
                mode = GL_TRIANGLE_STRIP;
                break;
            case PrimitiveType::PRIMITIVE_TYPE_TRI_FAN:
                mode = GL_TRIANGLE_FAN;
                break;
            default:
                continue;
        }

        GLenum type;

        switch (index_array.GetIndexType()) {
            case IndexDataType::INDEX_DATA_TYPE_INT8:
                type = GL_UNSIGNED_BYTE;
                break;
            case IndexDataType::INDEX_DATA_TYPE_INT16:
                type = GL_UNSIGNED_SHORT;
                break;
            case IndexDataType::INDEX_DATA_TYPE_INT32:
                type = GL_UNSIGNED_INT;
                break;
            default:
                // not supported by OpenGL
                cerr << "Error: Unsupported Index Type: " << index_array
                     << endl;
                cerr << "Mesh: " << *pMesh << endl;
                cerr << "Geometry: " << *pGeometry << endl;
                continue;
                break;
        }

        m_Buffers["index"] = buffer_id;

        DrawBatchContext& dbc = *(new DrawBatchContext);
        dbc.vao               = vao;
        dbc.mode              = mode;
        dbc.type              = type;
        dbc.count             = indexCount;
        dbc.transform         = pGeometryNode->GetCalculatedTransform();
        m_VAO.push_back(std::move(dbc));
        pGeometryNode = scene.GetNextGeometryNode();
    }
    return;
}

void OpenGLGraphicsManager::RenderBuffers() {
    static float rotateAngle = 0.0f;

    // rotateAngle += glm::pi<float>() / 1200;
    glm::mat4& worldMat = m_DrawFrameContext.m_worldMatrix;
    worldMat = glm::rotate(worldMat, rotateAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    worldMat = glm::rotate(worldMat, rotateAngle, glm::vec3(0.0f, 0.0f, 1.0f));

    CalculateCameraMatrix();
    CalculateLights();

    SetPerFrameShaderParameters();

    for (auto dbc : m_VAO) {
        glUseProgram(m_shaderProgram);
        SetPerBatchShaderParameters("objectLocalMatrix",
                                    &(*dbc.transform)[0][0]);
        glBindVertexArray(dbc.vao);
        glDrawElements(dbc.mode, dbc.count, dbc.type, 0);
    }
    return;
}

void OpenGLGraphicsManager::CalculateCameraMatrix() {
    auto& scene       = g_pSceneManager->GetSceneForRendering();
    auto  pCameraNode = scene.GetFirstCameraNode();

    glm::mat4& viewMat = m_DrawFrameContext.m_viewMatrix;
    if (pCameraNode) {
        viewMat = *pCameraNode->GetCalculatedTransform();
        viewMat = glm::inverse(viewMat);
    } else {
        glm::vec3 position(0, 0, 5);
        glm::vec3 lookAt(0, 0, 0);
        glm::vec3 up(0, 1, 0);
        viewMat = glm::lookAt(position, lookAt, up);
    }
    auto  pCamera = scene.GetCamera(pCameraNode->GetSceneObjectRef());
    float fieldOfView =
        dynamic_pointer_cast<SceneObjectPerspectiveCamera>(pCamera)->GetFov();
    const GfxConfiguration conf = g_pApp->GetConfiguration();

    float screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;

    m_DrawFrameContext.m_projectionMatrix = glm::perspective(
        fieldOfView, screenAspect, pCamera->GetNearClipDistance(),
        pCamera->GetFarClipDistance());
}

void OpenGLGraphicsManager::CalculateLights() {
    auto& scene      = g_pSceneManager->GetSceneForRendering();
    auto  pLightNode = scene.GetFirstLightNode();

    glm::vec3& lightPos   = m_DrawFrameContext.m_lightPosition;
    glm::vec4& lightColor = m_DrawFrameContext.m_lightColor;

    if (pLightNode) {
        lightPos = glm::vec3(0.0f);
        auto _lightPos =
            *pLightNode->GetCalculatedTransform() * glm::vec4(lightPos, 1.0f);
        lightPos = glm::vec3(_lightPos);

        auto pLight = scene.GetLight(pLightNode->GetSceneObjectRef());
        if (pLight) {
            lightColor = pLight->GetColor().Value;
        }
    } else {
        lightPos   = glm::vec3(10.0f, 10.0f, 10.0f);
        lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
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

    glLinkProgram(m_shaderProgram);
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &status);
    if (status != 1) {
        OutputLinkerErrorMessage(m_shaderProgram);
        return false;
    }
    return true;
}

}  // namespace My