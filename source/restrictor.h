#pragma once

#include "math2d.h"
#include "component.h"

class IRestrictor : public Component {
public:
    virtual bool can_pass(int2 coordinates) = 0;
};
