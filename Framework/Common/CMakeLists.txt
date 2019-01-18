add_library(Common
Allocator.cpp
AssetLoader.cpp
BaseApplication.cpp
GraphicsManager.cpp
MemoryManager.cpp
Scene.cpp
SceneObject.cpp
SceneManager.cpp
InputManager.cpp
DebugManager.cpp
GameLogic.cpp
Image.cpp
main.cpp
)

target_link_libraries(Common 
        GeomMath
        ${OPENGEX_LIBRARY} 
        ${OPENDDL_LIBRARY} 
        ${XG_LIBRARY} 
        ${ZLIB_LIBRARY}
)

__add_xg_platform_dependencies(Common)
