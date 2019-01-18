add_library(BulletPhysics BulletPhysicsManager.cpp)
target_link_libraries(BulletPhysics
    ${BULLET_DYNAMICS_LIBRARY} 
    ${BULLET_COLLISION_LIBRARY} 
    ${BULLET_LINEARMATH_LIBRARY}
)