#pragma once

#include "Interface.hpp"

namespace My
{
Interface IRuntimeMoudle
{
public:
  virtual ~IRuntimeMoudle(){};
  virtual int Initialize() = 0;
  virtual void Finalize() = 0;
  virtual void Tick() = 0;
};
} // namespace My
