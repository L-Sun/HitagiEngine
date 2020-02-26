#pragma once
#include "IRuntimeModule.hpp"

namespace My {

class GameLogic : public IRuntimeModule {
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

    virtual void OnCKeyDown();
    virtual void OnCKeyUp();
    virtual void OnCKey();
};
extern std::unique_ptr<GameLogic> g_GameLogic;
}  // namespace My
