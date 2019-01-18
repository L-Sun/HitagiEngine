#include <iostream>
#include "MyTest.hpp"
#include "SceneManager.hpp"
#include <chrono>
#include <thread>
#include "My/MyPhysicsManager.hpp"

using namespace std;
using namespace My;

int MyTest::Initialize() {
    int result;

    cout << "My Game Logic Initialize" << endl;
    cout << "Start Loading Game Scene" << endl;
    result = g_pSceneManager->LoadScene("Scene/test.ogex");

    return result;
}

void MyTest::Finalize() { cout << "MyTest Game Logic Finalize" << endl; }

void MyTest::Tick() { this_thread::sleep_for(std::chrono::milliseconds(60)); }

void MyTest::OnLeftKey() {}

void MyTest::OnDKey() {}
