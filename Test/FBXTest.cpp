#include <fbxsdk.h>
#include "AssetLoader.hpp"

/* Tab character ("\t") counter */
int numTabs = 0;

/**
 * Print the required number of tabs.
 */
void PrintTabs() {
    for (int i = 0; i < numTabs; i++) printf("\t");
}

/**
 * Return a string-based representation based on the attribute type.
 */
FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) {
    switch (type) {
        case FbxNodeAttribute::eUnknown:
            return "unidentified";
        case FbxNodeAttribute::eNull:
            return "null";
        case FbxNodeAttribute::eMarker:
            return "marker";
        case FbxNodeAttribute::eSkeleton:
            return "skeleton";
        case FbxNodeAttribute::eMesh:
            return "mesh";
        case FbxNodeAttribute::eNurbs:
            return "nurbs";
        case FbxNodeAttribute::ePatch:
            return "patch";
        case FbxNodeAttribute::eCamera:
            return "camera";
        case FbxNodeAttribute::eCameraStereo:
            return "stereo";
        case FbxNodeAttribute::eCameraSwitcher:
            return "camera switcher";
        case FbxNodeAttribute::eLight:
            return "light";
        case FbxNodeAttribute::eOpticalReference:
            return "optical reference";
        case FbxNodeAttribute::eOpticalMarker:
            return "marker";
        case FbxNodeAttribute::eNurbsCurve:
            return "nurbs curve";
        case FbxNodeAttribute::eTrimNurbsSurface:
            return "trim nurbs surface";
        case FbxNodeAttribute::eBoundary:
            return "boundary";
        case FbxNodeAttribute::eNurbsSurface:
            return "nurbs surface";
        case FbxNodeAttribute::eShape:
            return "shape";
        case FbxNodeAttribute::eLODGroup:
            return "lodgroup";
        case FbxNodeAttribute::eSubDiv:
            return "subdiv";
        default:
            return "unknown";
    }
}

/**
 * Print an attribute.
 */
void PrintAttribute(FbxNodeAttribute* pAttribute) {
    if (!pAttribute) return;

    FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
    FbxString attrName = pAttribute->GetName();
    PrintTabs();
    // Note: to retrieve the character array of a FbxString, use its
    // Buffer() method.
    printf("<attribute type='%s' name='%s'/>\n", typeName.Buffer(),
           attrName.Buffer());
}

/**
 * Print a node, its attributes, and all its children recursively.
 */
void PrintNode(FbxNode* pNode) {
    PrintTabs();
    const char* nodeName    = pNode->GetName();
    FbxDouble3  translation = pNode->LclTranslation.Get();
    FbxDouble3  rotation    = pNode->LclRotation.Get();
    FbxDouble3  scaling     = pNode->LclScaling.Get();

    // Print the contents of the node.
    printf(
        "<node name='%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' "
        "scaling='(%f, %f, %f)'>\n",
        nodeName, translation[0], translation[1], translation[2], rotation[0],
        rotation[1], rotation[2], scaling[0], scaling[1], scaling[2]);
    numTabs++;

    // Print the node's attributes.
    for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
        PrintAttribute(pNode->GetNodeAttributeByIndex(i));

    // Recursively print the children.
    for (int j = 0; j < pNode->GetChildCount(); j++)
        PrintNode(pNode->GetChild(j));

    numTabs--;
    PrintTabs();
    printf("</node>\n");
}

/**
 * Main function - loads the hard-coded fbx file,
 * and prints its contents in an xml format to stdout.
 */

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_pAssetLoader(new AssetLoader);
}  // namespace My

using namespace My;

int main(int argc, char** argv) {
    // Change the following filename to a suitable filename value.

    const char* lFilename = "E:/workspace/cpp/game_engine/Asset/Scene/test.fbx";

    // Initialize the SDK manager. This object handles all our memory
    // management.
    FbxManager* lSdkManager = FbxManager::Create();

    // Create the IO settings object.
    FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    lSdkManager->SetIOSettings(ios);

    // Create an importer using the SDK manager.
    FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

    // Use the first argument as the filename for the importer.
    if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings())) {
        printf("Call to FbxImporter::Initialize() failed.\n");
        printf("Error returned: %s\n\n",
               lImporter->GetStatus().GetErrorString());
        exit(-1);
    }

    // Create a new scene so that it can be populated by the imported file.
    FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

    // Import the contents of the file into the scene.
    lImporter->Import(lScene);

    // The file is imported; so get rid of the importer.
    lImporter->Destroy();

    // Print the nodes of the scene and their attributes recursively.
    // Note that we are not printing the root node because it should
    // not contain any attributes.
    FbxNode* lRootNode = lScene->GetRootNode();
    if (lRootNode) {
        for (int i = 0; i < lRootNode->GetChildCount(); i++)
            PrintNode(lRootNode->GetChild(i));
    }
    // Destroy the SDK manager and all the other objects it was handling.
    lSdkManager->Destroy();
    return 0;
}