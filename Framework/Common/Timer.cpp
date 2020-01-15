#include "Timer.hpp"

using namespace My;
using namespace std;

int Clock::Initialize() {
    mDeltaTime  = chrono::duration<double>::zero();
    mPausedTime = chrono::duration<double>::zero();
    mBaseTime   = chrono::high_resolution_clock::now();
    mPaused     = true;
    mStopTime   = mBaseTime;

    return 0;
}
void Clock::Finalize() {}

chrono::duration<double> Clock::deltaTime() const { return mDeltaTime; }

chrono::duration<double> Clock::totalTime() const {
    if (mPaused) {
        return (mStopTime - mBaseTime) - mPausedTime;
    } else {
        return (mTickTime - mBaseTime) - mPausedTime;
    }
}

chrono::high_resolution_clock::time_point Clock::tickTime() const {
    return mTickTime;
}

void Clock::Tick() {
    if (mPaused) {
        mDeltaTime = chrono::duration<double>::zero();
        return;
    }
    mTickTime  = chrono::high_resolution_clock::now();
    mDeltaTime = mTickTime - mPrevTime;
    mPrevTime  = mTickTime;
}

void Clock::Start() {
    if (mPaused) {
        auto now = chrono::high_resolution_clock::now();
        mPausedTime += now - mStopTime;
        mPrevTime = now;
        mPaused   = false;
    }
}

void Clock::Pause() {
    if (!mPaused) {
        mStopTime = chrono::high_resolution_clock::now();
        mPaused   = true;
    }
}

void Clock::Reset() {
    mBaseTime = chrono::high_resolution_clock::now();
    mPaused   = true;
}