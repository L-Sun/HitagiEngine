#include "Timer.hpp"

using namespace My;
using namespace std;

int Clock::Initialize() {
    mDeltaTime  = chrono::duration<double>::zero();
    mPausedTime = chrono::duration<double>::zero();
    mBaseTime   = chrono::high_resolution_clock::now();
    mPaused     = true;
    mStopTime   = mBaseTime;

    return 1;
}
void Clock::Finalize() {}

float Clock::deltaTime() const {
    return static_cast<float>(mDeltaTime.count());
}

float Clock::totalTime() const {
    if (mPaused) {
        return static_cast<float>(
            ((mStopTime - mBaseTime) - mPausedTime).count());
    } else {
        return static_cast<float>(
            ((mCurrTime - mBaseTime) - mPausedTime).count());
    }
}

void Clock::Tick() {
    if (mPaused) {
        mDeltaTime = chrono::duration<double>::zero();
        return;
    }
    mCurrTime  = chrono::high_resolution_clock::now();
    mDeltaTime = mCurrTime - mPrevTime;
    mPrevTime  = mCurrTime;
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