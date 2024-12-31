#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include <GLFW/glfw3.h>

class InputHandler {
public:
    InputHandler();
    ~InputHandler();

    void update();
    void processInput();
};

#endif // INPUTHANDLER_H