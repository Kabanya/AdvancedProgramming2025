#pragma once

#include "math2d.h"

class IRestrictor {
public:
    virtual bool can_pass(int2 coordinates) = 0;
};
