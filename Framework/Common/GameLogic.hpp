#pragma once
#include "IRuntimeModule.hpp"

namespace My {

class GameLogic : implements IRuntimeModule {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    virtual void OnUpKeyDown();
    virtual void OnUpKeyUp();
    virtual void OnUpKey();

    virtual void OnDownKeyDown();
    virtual void OnDownKeyUp();
    virtual void OnDownKey();

    virtual void OnLeftKeyDown();
    virtual void OnLeftKeyUp();
    virtual void OnLeftKey();

    virtual void OnRightKeyDown();
    virtual void OnRightKeyUp();
    virtual void OnRightKey();

    virtual void OnRKeyDown();
    virtual void OnRKeyUp();
    virtual void OnRKey();

    virtual void OnDKeyDown();
    virtual void OnDKeyUp();
    virtual void OnDKey();
};
extern GameLogic* g_pGameLogic;
}  // namespace My