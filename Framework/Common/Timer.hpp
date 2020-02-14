#pragma once
#include <chrono>
#include "IRuntimeModule.hpp"

namespace My {
class Clock : public IRuntimeModule {
public:
    virtual int  Initialize();
    virtual void Tick();
    virtual void Finalize();

    std::chrono::duration<double> totalTime() const;
    std::chrono::duration<double> deltaTime() const;

    std::chrono::high_resolution_clock::time_point tickTime() const;

    void Reset();
    void Start();
    void Pause();

private:
    std::chrono::duration<double> mDeltaTime;
    std::chrono::duration<double> mPausedTime;

    std::chrono::high_resolution_clock::time_point mBaseTime;
    std::chrono::high_resolution_clock::time_point mStopTime;
    std::chrono::high_resolution_clock::time_point mPrevTime;
    std::chrono::high_resolution_clock::time_point mTickTime;

    bool mPaused = true;
};

}  // namespace My
