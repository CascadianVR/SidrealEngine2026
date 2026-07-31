#pragma once

#define SDL_MAIN_HANDLED
#include <SDL.h>

class Window {
public:
	Window() = delete;
	Window(int width, int height, const char* title);
	~Window();
	
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	void Show() const;
	void Hide() const;
	void PollEvents();
	void ClearEvents();
	bool ShouldClose() const;
	bool WasResized() const;
	unsigned int GetWidth() const { return m_width; }
	unsigned int GetHeight() const { return m_height; }
	SDL_Window* GetSDLWindow() const;
private:
	bool m_windowResized = false;
	int m_height = 0;
	int m_width = 0;
	int m_shouldClose = 0;
	
	SDL_Event m_event;
	SDL_Window* m_window = nullptr;
	const char* m_title = nullptr;
};