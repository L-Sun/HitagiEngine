#include "Timer.hpp"

using namespace My;

int Clock::Initialize() {
    mDeltaTime  = std::chrono::duration<double>::zero();
    mPausedTime = std::chrono::duration<double>::zero();
    mBaseTime   = std::chrono::high_resolution_clock::now();
    mPaused     = true;
    mStopTime   = mBaseTime;

    return 0;
}
void Clock::Finalize() {}

std::chrono::duration<double> Clock::deltaTime() const { return mDeltaTime; }

std::chrono::duration<double> Clock::totalTime() const {
    if (mPaused) {
        return (mStopTime - mBaseTime) - mPausedTime;
    } else {
        return (mTickTime - mBaseTime) - mPausedTime;
    }
}

std::chrono::high_resolution_clock::time_point Clock::tickTime() const {
    return mTickTime;
}

void Clock::Tick() {
    if (mPaused) {
        mDeltaTime = std::chrono::duration<double>::zero();
        return;
    }
    mTickTime  = std::chrono::high_resolution_clock::now();
    mDeltaTime = mTickTime - mPrevTime;
    mPrevTime  = mTickTime;
}

void Clock::Start() {
    if (mPaused) {
        auto now = std::chrono::high_resolution_clock::now();
        mPausedTime += now - mStopTime;
        mPrevTime = now;
        mPaused   = false;
    }
}

void Clock::Pause() {
    if (!mPaused) {
        mStopTime = std::chrono::high_resolution_clock::now();
        mPaused   = true;
    }
}

void Clock::Reset() {
    mBaseTime = std::chrono::high_resolution_clock::now();
    mPaused   = true;
}