#include "Timer.hpp"

using namespace My;
using namespace std::chrono;

int Clock::Initialize() {
    mDeltaTime  = duration<double>::zero();
    mPausedTime = duration<double>::zero();
    mBaseTime   = high_resolution_clock::now();
    mStopped    = true;
    mStopTime   = mBaseTime;

    return 1;
}

float Clock::deltaTime() const {
    return static_cast<float>(mDeltaTime.count());
}

float Clock::totalTime() const {
    if (mStopped) {
        return static_cast<float>(
            ((mStopTime - mBaseTime) - mPausedTime).count());
    } else {
        return static_cast<float>(
            ((mCurrTime - mBaseTime) - mPausedTime).count());
    }
}

void Clock::Tick() {
    if (mStopped) {
        mDeltaTime = duration<double>::zero();
        return;
    }
    mCurrTime  = high_resolution_clock::now();
    mDeltaTime = mCurrTime - mPrevTime;
    mPrevTime  = mCurrTime;
}

void Clock::Start() {
    if (mStopped) {
        auto now = high_resolution_clock::now();
        mPausedTime += now - mStopTime;
        mPrevTime = now;
        mStopped  = false;
    }
}

void Clock::Stop() {
    if (!mStopped) {
        mStopTime = high_resolution_clock::now();
        mStopped  = true;
    }
}

void Clock::Reset() {
    mBaseTime = high_resolution_clock::now();
    mStopped  = true;
}