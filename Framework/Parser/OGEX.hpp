#include "SceneParser.hpp"
#include "OpenGEX.h"

namespace My {
class OgexParser : public SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const Buffer& buf);

private:
    bool m_bUpIsYAxis;
    void ConvertOddlStructureToSceneNode(
        const ODDL::Structure&          structure,
        std::shared_ptr<BaseSceneNode>& base_node, Scene& scene);
};
}  // namespace My
