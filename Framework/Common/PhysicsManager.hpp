#include "IRuntimeModule.hpp"

namespace My {
class PhysicsManager : implements IRuntimeModule {
public:
    virtual ~PhysicsManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
};
}  // namespace My
