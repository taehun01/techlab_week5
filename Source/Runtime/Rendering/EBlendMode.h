#pragma once

#include "Runtime/Core/IntTypes.h"

enum class EBlendMode : uint8
{
  None,
  Opaque,
  Masked,
  Translucent,
  Additive,
  PremultipliedAlpha,
};
