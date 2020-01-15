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
        return (mCurrTime - mBaseTime) - mPausedTime;
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