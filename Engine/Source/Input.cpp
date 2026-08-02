#include "Input.h"

#include "Logger.h"

void Input::Initialize(Window* window)
{
    m_window = window;
    SDL_Window* sdlWindow = window->GetSDLWindow();
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Input::Update()
{
    const Uint8* currentKeyStates = SDL_GetKeyboardState(nullptr);
    
    if (currentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        m_window->SetShouldClose(true);
    }
    
    wKeyPressed = currentKeyStates[SDL_SCANCODE_W];
    aKeyPressed = currentKeyStates[SDL_SCANCODE_A];
    sKeyPressed = currentKeyStates[SDL_SCANCODE_S];
    dKeyPressed = currentKeyStates[SDL_SCANCODE_D]; 
    qKeyPressed = currentKeyStates[SDL_SCANCODE_Q];
    eKeyPressed = currentKeyStates[SDL_SCANCODE_E];
    
    const Uint32 mouseState = SDL_GetMouseState(&mousePosX, &mousePosY);
    SDL_GetRelativeMouseState(&deltaMouseX, &deltaMouseY);
    
    leftMouseButtonPressed = mouseState & SDL_BUTTON(SDL_BUTTON_LEFT);
    rightMouseButtonPressed = mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT);
    middleMouseButtonPressed = mouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE);
    
}