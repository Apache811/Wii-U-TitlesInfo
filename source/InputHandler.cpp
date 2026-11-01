#include "InputHandler.hpp"

#include <vpad/input.h>

InputState InputHandler::Poll()
{
    InputState result;

    VPADStatus status;
    VPADReadError error;
    if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0)
    {
        result.up = (status.trigger & VPAD_BUTTON_UP) != 0;
        result.down = (status.trigger & VPAD_BUTTON_DOWN) != 0;
        result.pageUp = (status.trigger & VPAD_BUTTON_L) != 0;
        result.pageDown = (status.trigger & VPAD_BUTTON_R) != 0;
        result.refresh = (status.trigger & VPAD_BUTTON_X) != 0;
        result.search = (status.trigger & VPAD_BUTTON_Y) != 0;
        result.filterPrevious = (status.trigger & VPAD_BUTTON_ZL) != 0;
        result.filterNext = (status.trigger & VPAD_BUTTON_ZR) != 0;
    }

    return result;
}
