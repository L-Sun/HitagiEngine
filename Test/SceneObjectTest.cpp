#include <iostream>
#include "SceneObject.hpp"
#include "SceneNode.hpp"

using namespace My;

int32_t main(int32_t argc, char** argv) {
    int32_t                               result = 0;
    std::shared_ptr<SceneObjectGeometry>  soGeometry(new SceneObjectGeometry());
    std::shared_ptr<SceneObjectOmniLight> soOmniLight(
        new SceneObjectOmniLight());
    std::shared_ptr<SceneObjectSpotLight> soSpotLight(
        new SceneObjectSpotLight());
    std::shared_ptr<SceneObjectOrthogonalCamera> soOrthogonalCamera(
        new SceneObjectOrthogonalCamera());
    std::shared_ptr<SceneObjectPerspectiveCamera> soPerspectiveCamera(
        new SceneObjectPerspectiveCamera());

    std::shared_ptr<SceneObjectMesh>     soMesh(new SceneObjectMesh());
    std::shared_ptr<SceneObjectMaterial> soMaterial(new SceneObjectMaterial());

    soGeometry->AddMesh(soMesh);

    std::cout << *soGeometry << std::endl;
    std::cout << *soMaterial << std::endl;
    std::cout << *soOmniLight << std::endl;
    std::cout << *soSpotLight << std::endl;
    std::cout << *soOrthogonalCamera << std::endl;
    std::cout << *soPerspectiveCamera << std::endl;

    SceneEmptyNode                     snEmpty;
    std::shared_ptr<SceneGeometryNode> snGeometry(new SceneGeometryNode());
    std::shared_ptr<SceneLightNode>    snLight(new SceneLightNode());
    std::shared_ptr<SceneCameraNode>   snCamera(new SceneCameraNode());

    snGeometry->AddSceneObjectRef(soGeometry);
    snLight->AddSceneObjectRef(soSpotLight);
    snCamera->AddSceneObjectRef(soOrthogonalCamera);

    snEmpty.AppendChild(std::move(snGeometry));
    snEmpty.AppendChild(std::move(snLight));
    snEmpty.AppendChild(std::move(snCamera));

    std::cout << snEmpty << std::endl;

    return result;
}
