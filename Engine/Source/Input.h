#pragma once

#include "Window.h"

class Input {
public:
    static void Initialize(Window* window);
    static void Update();
    
    // Key inputs
    static inline bool wKeyPressed = false;
    static inline bool aKeyPressed = false;
    static inline bool sKeyPressed = false;
    static inline bool dKeyPressed = false;
    static inline bool qKeyPressed = false;
    static inline bool eKeyPressed = false;
    
    // Mouse inputs
    static inline __readonly bool leftMouseButtonPressed = false;
    static inline __readonly bool rightMouseButtonPressed = false;
    static inline __readonly bool middleMouseButtonPressed = false;
    static inline __readonly int mousePosX;
    static inline __readonly int mousePosY;
    static inline __readonly int mousePosXLast;
    static inline __readonly int mousePosYLast;
    static inline __readonly int deltaMouseX = 0;
    static inline __readonly int deltaMouseY = 0;
private:
    static inline Window* m_window = nullptr;
    static inline SDL_Event event;
};