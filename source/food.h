#pragma once

#include "component.h"
#include "game_object.h"

class IFood : public Component {

public:
    virtual void on_consume(GameObjectPtr consumer) = 0;
};