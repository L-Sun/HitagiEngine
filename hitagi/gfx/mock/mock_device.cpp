export module gfx.mock:device;
import std;
import utils;
import math;
import core;
import gfx.base;

export namespace hitagi::gfx {

class MockDevice : public Device {
public:
    MockDevice(std::string_view name);

    void Tick() final {}

private:
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MockDevice::MockDevice(std::string_view name) : Device(Device::Type::Mock, name) {
}

}  // namespace hitagi::gfx
