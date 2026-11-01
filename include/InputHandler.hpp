#pragma once

struct InputState
{
    bool up = false;
    bool down = false;
    bool pageUp = false;
    bool pageDown = false;
    bool refresh = false;
    bool filterPrevious = false;
    bool filterNext = false;
    bool search = false;
};

#include <vpad/input.h>

class InputHandler
{
public:
    InputState Poll();
private:
};
