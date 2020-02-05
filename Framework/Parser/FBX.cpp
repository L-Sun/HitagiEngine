#include <fbxsdk.h>
#include <algorithm>
#include "FBX.hpp"
#include "geommath.hpp"

using namespace My;

class MemoryStream : public FbxStream {
public:
    MemoryStream(const Buffer& buffer, int readID = -1, int writeID = -1)
        : m_pData(buffer.GetData()),
          m_szData(buffer.GetDataSize()),
          m_readerID(readID),
          m_writeID(writeID) {}
    ~MemoryStream() { Close(); }

    virtual EState GetState() { return m_state; }

    virtual bool Open(void* pStreamData) {
        m_state = EState::eOpen;
        m_pos   = 0;
        return true;
    }

    virtual bool Close() {
        m_state = EState::eClosed;
        return true;
    }
    virtual bool Flush() { return true; }
    virtual int  Write(const void* pData, int pSize) {
        m_errorCode = 1;
        return 0;
    }
    virtual int Read(void* pData, int pSize) const {
        int read_size = pSize <= m_szData - m_pos ? pSize : m_szData - m_pos;
        std::memcpy(pData, m_pData + m_pos, read_size);
        const_cast<MemoryStream&>(*this).m_pos += read_size;
        return read_size;
    };
    virtual int  GetReaderID() const { return m_readerID; }
    virtual int  GetWriterID() const { return m_writeID; }
    virtual void Seek(const FbxInt64& pOffset, const FbxFile::ESeekPos& pSeekPos) {
        switch (pSeekPos) {
            case FbxFile::ESeekPos::eCurrent:
                m_pos += pOffset;
                break;
            case FbxFile::ESeekPos::eEnd:
                m_pos = m_szData - pOffset;
                break;
            case FbxFile::ESeekPos::eBegin:
            default:
                m_pos = pOffset;
                break;
        }
    }
    virtual long GetPosition() const { return m_state == EState::eOpen ? m_pos : 0; }
    virtual void SetPosition(long pPosition) { m_pos = pPosition; }
    virtual int  GetError() const { return m_errorCode; }
    virtual void ClearError() { m_errorCode = 0; }

private:
    const uint8_t* m_pData;
    size_t         m_szData;
    size_t         m_pos;
    EState         m_state;

    int m_errorCode;
    int m_readerID;
    int m_writeID;
};

std::shared_ptr<SceneObjectMesh> addMesh(FbxMesh* _pMesh, Scene& scene) {
    auto pMesh = std::make_shared<SceneObjectMesh>();
    pMesh->SetPrimitiveType(PrimitiveType::kTRI_LIST);

    int indicesSize   = _pMesh->GetPolygonCount() * 3;  // now just for triangle
    int ctrlPointSize = _pMesh->GetControlPointsCount();
    if (indicesSize == 0) {
        std::cerr << "[FbxParser] parser unsupport dot and line now. Skipping the Mesh: " << _pMesh->GetName() << std::endl;
        return nullptr;
    }
    std::vector<vec3f> posArray(ctrlPointSize);
    std::vector<int>   indicesArray(indicesSize);
    std::vector<vec3f> normalArray(indicesSize);
    std::vector<vec2f> uvArray(indicesSize);
    std::vector<vec4f> colorArray(indicesSize);
    std::vector<vec3f> tangentArray(indicesSize);

    // Read position
    if (int pointCount = _pMesh->GetControlPointsCount(); pointCount > 0) {
        for (int i = 0; i < pointCount; i++) {
            const auto& vertPos = _pMesh->GetControlPointAt(i);
            posArray[i]         = vec3f(vertPos[0], vertPos[1], vertPos[2]);
        }
        pMesh->AddVertexArray(std::move(
            SceneObjectVertexArray("POSITION", 0, VertexDataType::kFLOAT3, posArray.data(), pointCount)));
    }

    int polygonCount = _pMesh->GetPolygonCount();
    for (int polygon = 0, vertCounter = 0; polygon < polygonCount; polygon++) {
        int polyVertCount = _pMesh->GetPolygonSize(polygon);
        for (int polyVert = 0; polyVert < polyVertCount; polyVert++, vertCounter++) {
            int ctrlPointIndex = _pMesh->GetPolygonVertex(polygon, polyVert);
            // Read index
            indicesArray[vertCounter] = ctrlPointIndex;

            // Read Normal
            if (auto _pNormal = _pMesh->GetElementNormal()) {
                auto _mapMode = _pNormal->GetMappingMode();
                auto _refMode = _pNormal->GetReferenceMode();

                int directIndex = -1;
                if (_mapMode == FbxGeometryElement::eByControlPoint) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = ctrlPointIndex;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pNormal->GetIndexArray().GetAt(ctrlPointIndex);
                    }
                } else if (_mapMode == FbxGeometryElement::eByPolygonVertex) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = vertCounter;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pNormal->GetIndexArray().GetAt(vertCounter);
                    }
                }
                if (directIndex != -1) {
                    normalArray[vertCounter] = vec3f(
                        _pNormal->GetDirectArray().GetAt(directIndex)[0],
                        _pNormal->GetDirectArray().GetAt(directIndex)[1],
                        _pNormal->GetDirectArray().GetAt(directIndex)[2]);
                }
            }

            // Read UV
            if (auto _pUV = _pMesh->GetElementUV()) {
                auto _mapMode = _pUV->GetMappingMode();
                auto _refMode = _pUV->GetReferenceMode();

                int directIndex = -1;
                if (_mapMode == FbxGeometryElement::eByControlPoint) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = ctrlPointIndex;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pUV->GetIndexArray().GetAt(ctrlPointIndex);
                    }
                } else if (_mapMode == FbxGeometryElement::eByPolygonVertex) {
                    if (_refMode == FbxGeometryElement::eDirect ||
                        _refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pMesh->GetTextureUVIndex(polygon, polyVert);
                    }
                }

                if (directIndex != -1) {
                    uvArray[vertCounter] = vec2f(
                        _pUV->GetDirectArray().GetAt(directIndex)[0],
                        _pUV->GetDirectArray().GetAt(directIndex)[1]);
                }
            }

            // Read Color
            if (auto _pColor = _pMesh->GetElementVertexColor()) {
                auto _mapMode = _pColor->GetMappingMode();
                auto _refMode = _pColor->GetReferenceMode();

                int directIndex = -1;
                if (_mapMode == FbxGeometryElement::eByControlPoint) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = ctrlPointIndex;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pColor->GetIndexArray().GetAt(ctrlPointIndex);
                    }
                } else if (_mapMode == FbxGeometryElement::eByPolygonVertex) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = vertCounter;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pColor->GetIndexArray().GetAt(vertCounter);
                    }
                }

                if (directIndex != -1) {
                    colorArray[vertCounter] = vec4f(
                        _pColor->GetDirectArray().GetAt(directIndex).mRed,
                        _pColor->GetDirectArray().GetAt(directIndex).mGreen,
                        _pColor->GetDirectArray().GetAt(directIndex).mBlue,
                        _pColor->GetDirectArray().GetAt(directIndex).mAlpha);
                }
            }

            // Read Tangent
            if (auto _pTangent = _pMesh->GetElementNormal()) {
                auto _mapMode = _pTangent->GetMappingMode();
                auto _refMode = _pTangent->GetReferenceMode();

                int directIndex = -1;
                if (_mapMode == FbxGeometryElement::eByControlPoint) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = ctrlPointIndex;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pTangent->GetIndexArray().GetAt(ctrlPointIndex);
                    }
                } else if (_mapMode == FbxGeometryElement::eByPolygonVertex) {
                    if (_refMode == FbxGeometryElement::eDirect) {
                        directIndex = vertCounter;
                    } else if (_refMode == FbxGeometryElement::eIndexToDirect) {
                        directIndex = _pTangent->GetIndexArray().GetAt(vertCounter);
                    }
                }
                if (directIndex != -1) {
                    normalArray[vertCounter] = vec3f(
                        _pTangent->GetDirectArray().GetAt(directIndex)[0],
                        _pTangent->GetDirectArray().GetAt(directIndex)[1],
                        _pTangent->GetDirectArray().GetAt(directIndex)[2]);
                }
            }
        }
    }

    pMesh->AddIndexArray(std::move(
        SceneObjectIndexArray(0, 0, IndexDataType::kINT32, indicesArray.data(), indicesSize)));
    if (_pMesh->GetElementNormalCount() > 0)
        pMesh->AddVertexArray(std::move(
            SceneObjectVertexArray("NORMAL", 0, VertexDataType::kFLOAT3, normalArray.data(), indicesSize)));

    if (_pMesh->GetElementVertexColorCount() > 0)
        pMesh->AddVertexArray(std::move(
            SceneObjectVertexArray("COLOR", 0, VertexDataType::kFLOAT4, colorArray.data(), indicesSize)));

    if (_pMesh->GetElementTangentCount() > 0)
        pMesh->AddVertexArray(std::move(
            SceneObjectVertexArray("TANGENT", 0, VertexDataType::kFLOAT3, tangentArray.data(), indicesSize)));

    if (_pMesh->GetElementUVCount() > 0)
        pMesh->AddVertexArray(std::move(
            SceneObjectVertexArray("TEXCOORD", 0, VertexDataType::kFLOAT2, uvArray.data(), indicesSize)));

    // Read Material Indices
    // for (int i = 0; i < _pMesh->GetElementMaterialCount(); i++) {
    //     if (auto _pMaterial = _pMesh->GetElementMaterial(i)) {
    //         int* indicesArray = new int[indicesSize];
    //         for (int j = 0; j < indicesSize; j++)
    //             indicesArray[j] = _pMaterial->GetIndexArray()[j];

    //         // pMesh have a index array already, so the index of array is i + 1.
    //         pMesh->AddIndexArray(std::move(
    //             SceneObjectIndexArray(i + 1, 0, IndexDataType::kINT32, indicesArray, indicesSize)));
    //     }
    // }

    return pMesh;
}

std::unique_ptr<Scene> FbxParser::Parse(const Buffer& buf) {
    std::unique_ptr<Scene> pScene(new Scene("FBX Scene"));

    // Initialize the SDK manager. This object handles all our memory
    // management.
    FbxManager* pSdkManager = FbxManager::Create();

    // Create the IO settings object.
    FbxIOSettings* ios = FbxIOSettings::Create(pSdkManager, IOSROOT);
    pSdkManager->SetIOSettings(ios);

    // Create an importer using the SDK manager.
    FbxImporter* pImporter = FbxImporter::Create(pSdkManager, "");

    FbxIOPluginRegistry* r = pSdkManager->GetIOPluginRegistry();
    MemoryStream         ms(buf, r->FindReaderIDByExtension("fbx"));

    if (!pImporter->Initialize(&ms, nullptr, -1, pSdkManager->GetIOSettings())) {
        std::cerr << "[FbxParser] Call to FbxImporter::Initialize() failed." << std::endl;
        std::cerr << "Error returned: " << pImporter->GetStatus().GetErrorString() << std::endl;
        return nullptr;
    }

    FbxScene* pFbxScene = FbxScene::Create(pSdkManager, "Fbx Scene");
    // Import the contents of the file into the scene.
    pImporter->Import(pFbxScene);
    pImporter->Destroy();

    // triangulate all mesh
    FbxGeometryConverter geometryConverter(pSdkManager);
    geometryConverter.Triangulate(pFbxScene, true);

    // Init Material
    for (size_t i = 0; i < pFbxScene->GetMaterialCount(); i++) {
        auto _pMaterial = pFbxScene->GetMaterial(i);
        auto pMaterial  = std::make_shared<SceneObjectMaterial>();
        pMaterial->SetName(_pMaterial->GetName());
        if (_pMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) {
            auto _Phong     = reinterpret_cast<FbxSurfacePhong*>(_pMaterial);
            auto _diffuse   = _Phong->Diffuse.Get();
            auto _specular  = _Phong->Specular.Get();
            auto _emission  = _Phong->Emissive.Get();
            auto _shininess = _Phong->Shininess.Get();
            auto _opacity   = 1.0 - _Phong->TransparencyFactor.Get();
            pMaterial->SetColor("diffuse", vec4f(_diffuse[0], _diffuse[1], _diffuse[2], 1.0f));
            pMaterial->SetColor("specular", vec4f(_specular[0], _specular[1], _specular[2], 1.0f));
            pMaterial->SetColor("emission", vec4f(_emission[0], _emission[1], _emission[2], 1.0f));
            pMaterial->SetColor("opacity", vec4f(_opacity));
            pMaterial->SetColor("transparency", vec4f(1.0f - _opacity));
            pMaterial->SetParam("specular_power", static_cast<float>(_shininess));
        }

        pScene->Materials[_pMaterial->GetName()] = pMaterial;
    }

    auto getTrans = [&](FbxNode* _pNode) -> mat4f {
        auto  _scaling    = _pNode->LclScaling.Get().Buffer();
        auto  _rotation   = _pNode->LclRotation.Get().Buffer();
        auto  _traslation = _pNode->LclTranslation.Get().Buffer();
        mat4f trans(1.0f);
        trans = scale(trans, vec3f(_scaling));
        trans = rotate(trans,
                       radians(static_cast<float>(_rotation[0])),
                       radians(static_cast<float>(_rotation[1])),
                       radians(static_cast<float>(_rotation[2])));
        std::cout << _pNode->GetName() << vec3f(static_cast<float>(_rotation[0]), static_cast<float>(_rotation[1]), static_cast<float>(_rotation[2]))
                  << std::endl;

        trans = translate(trans, vec3f(_traslation));

        return trans;
    };

    auto processGeometry = [&](FbxNode* _pNode) -> std::shared_ptr<SceneGeometryNode> {
        auto _pGeometry = _pNode->GetGeometry();
        if (pScene->Geometries.find(_pGeometry->GetName()) == pScene->Geometries.end()) {
            auto _pMesh = _pNode->GetMesh();

            // Add new Mesh to geometry
            // TODO: LOD
            auto pGeometry = std::make_shared<SceneObjectGeometry>();
            auto pMesh     = addMesh(_pMesh, *pScene);
            if (pMesh) {
                pGeometry->AddMesh(addMesh(_pMesh, *pScene));
                pScene->Geometries[_pGeometry->GetName()] = pGeometry;
            } else {
                return nullptr;
            }
        }

        auto pGeometryNode = std::make_shared<SceneGeometryNode>(_pNode->GetName());
        pGeometryNode->SetVisibility(_pNode->GetVisibility());
        pGeometryNode->SetIfCastShadow(true);
        pGeometryNode->SetIfMotionBlur(false);
        pGeometryNode->AddSceneObjectRef(_pGeometry->GetName());
        for (size_t i = 0; i < _pNode->GetMaterialCount(); i++) {
            pGeometryNode->AddMaterialRef(_pNode->GetMaterial(i)->GetName());
        }

        pGeometryNode->AppendTransform(std::make_shared<SceneObjectTransform>(getTrans(_pNode)));

        pScene->GeometryNodes.emplace(_pNode->GetName(), pGeometryNode);
        return pGeometryNode;
    };

    auto processCamera = [&](FbxNode* _pNode) -> std::shared_ptr<SceneCameraNode> {
        auto _pCamera = _pNode->GetCamera();
        if (pScene->Cameras.find(_pCamera->GetName()) == pScene->Cameras.end()) {
            auto pCamera = std::make_shared<SceneObjectPerspectiveCamera>();
            pCamera->SetParam("fov", static_cast<float>(_pCamera->FieldOfView.Get()));
            pCamera->SetParam("near", static_cast<float>(_pCamera->NearPlane.Get()));
            pCamera->SetParam("far", static_cast<float>(_pCamera->FarPlane.Get()));
            pScene->Cameras[_pCamera->GetName()] = pCamera;
        }

        auto pCameraNode = std::make_shared<SceneCameraNode>(_pNode->GetName());
        pCameraNode->AppendTransform(std::make_shared<SceneObjectTransform>(getTrans(_pNode)));
        pCameraNode->AddSceneObjectRef(_pCamera->GetName());
        pScene->CameraNodes.emplace(_pNode->GetName(), pCameraNode);

        return pCameraNode;
    };

    auto processLight = [&](FbxNode* _pNode) -> std::shared_ptr<SceneLightNode> {
        auto _pLight = _pNode->GetLight();
        if (pScene->Lights.find(_pLight->GetName()) == pScene->Lights.end()) {
            std::shared_ptr<SceneObjectLight> pLight;
            switch (_pLight->LightType.Get()) {
                case FbxLight::EType::ePoint:
                    pLight = std::make_shared<SceneObjectOmniLight>();
                    break;
                case FbxLight::EType::eSpot:
                    pLight = std::make_shared<SceneObjectSpotLight>();
                    break;
                default:
                    break;
            }

            vec3f color(_pLight->Color.Get().Buffer());
            pLight->SetColor("light", vec4f(color, 1.0f));
            pLight->SetParam("intensity", static_cast<float>(_pLight->Intensity.Get()));
            pLight->SetAttenuation(DefaultAttenFunc);
            pScene->Lights[_pLight->GetName()] = pLight;
        }

        auto pLightNode = std::make_shared<SceneLightNode>(_pNode->GetName());
        pLightNode->AppendTransform(std::make_shared<SceneObjectTransform>(getTrans(_pNode)));
        pLightNode->AddSceneObjectRef(_pLight->GetName());
        pScene->LightNodes.emplace(_pNode->GetName(), pLightNode);

        return pLightNode;
    };

    std::function<std::shared_ptr<BaseSceneNode>(FbxNode*)>
        convert = [&](FbxNode* _pNode) -> std::shared_ptr<BaseSceneNode> {
        std::shared_ptr<BaseSceneNode> pNode = std::make_shared<BaseSceneNode>();
        ;
        // the node is root.
        if (auto attribute = _pNode->GetNodeAttribute()) {
            switch (attribute->GetAttributeType()) {
                case FbxNodeAttribute::eUnknown:
                    break;
                case FbxNodeAttribute::eNull:
                    break;
                case FbxNodeAttribute::eMarker:
                    break;
                case FbxNodeAttribute::eSkeleton:
                    break;
                case FbxNodeAttribute::eMesh:
                    pNode = processGeometry(_pNode);
                    break;
                case FbxNodeAttribute::eNurbs:
                    break;
                case FbxNodeAttribute::ePatch:
                    break;
                case FbxNodeAttribute::eCamera:
                    pNode = processCamera(_pNode);
                    break;
                case FbxNodeAttribute::eCameraStereo:
                    break;
                case FbxNodeAttribute::eCameraSwitcher:
                    break;
                case FbxNodeAttribute::eLight:
                    pNode = processLight(_pNode);
                    break;
                case FbxNodeAttribute::eOpticalReference:
                    break;
                case FbxNodeAttribute::eOpticalMarker:
                    break;
                case FbxNodeAttribute::eNurbsCurve:
                    break;
                case FbxNodeAttribute::eTrimNurbsSurface:
                    break;
                case FbxNodeAttribute::eBoundary:
                    break;
                case FbxNodeAttribute::eNurbsSurface:
                    break;
                case FbxNodeAttribute::eShape:
                    break;
                case FbxNodeAttribute::eLODGroup:
                    break;
                case FbxNodeAttribute::eSubDiv:
                    break;
                default:
                    break;
            }
        }

        for (size_t i = 0; i < _pNode->GetChildCount(); i++) {
            pNode->AppendChild(convert(_pNode->GetChild(i)));
        }
        return pNode;
    };

    pScene->SceneGraph = convert(pFbxScene->GetRootNode());
    pSdkManager->Destroy();
    return pScene;
}