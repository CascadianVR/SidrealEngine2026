#include "Window.h"
#include "Logger.h"

Window::Window(const int width, const int height, const char* title)
{
	m_width = width;
	m_height = height;
	m_title = title;
	m_windowResized = false;
	
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		Logger::Error("Failed to initialize SDL");
		return;
	}

	// Hints
	SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "1");
	
	m_window = SDL_CreateWindow(
		title,                  // Window title
		SDL_WINDOWPOS_CENTERED, // Initial x position
		SDL_WINDOWPOS_CENTERED, // Initial y position
		width,                  // Width, in pixels
		height,                 // Height, in pixels
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN // Flags (ensures visibility)
	);

	if (m_window == nullptr) {
		Logger::Error("Failed to create SDL window");
		SDL_Quit();
		return;
	}

	Logger::Success("Created Window");
}

Window::~Window()
{
	SDL_DestroyWindow(m_window);
	SDL_Quit();
}

bool Window::ShouldClose() const
{
	return m_shouldClose;
}

bool Window::WasResized() const
{
	return m_windowResized;
}

void Window::Show() const
{
	SDL_ShowWindow(m_window);
}

void Window::Hide() const
{
	SDL_HideWindow(m_window);
}

void Window::PollEvents()
{
	m_windowResized = false;
	while (SDL_PollEvent(&m_event))
	{
		switch (m_event.type)
		{
			case SDL_QUIT:
				break;
				
			case SDL_WINDOWEVENT:
			{
				if (m_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					m_windowResized = true;
					SDL_GetWindowSize(m_window, &m_width, &m_height);
				}
				else if (m_event.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					m_shouldClose = true;
				}
			}

			case SDL_KEYDOWN:
				// Handle key press
				break;

			case SDL_KEYUP:
				// Handle key release
				break;

			case SDL_MOUSEMOTION:
				// Handle mouse movement
				break;

			case SDL_MOUSEBUTTONDOWN:
				// Handle mouse button
				break;
		}
	}
}

void Window::ClearEvents()
{
	m_event.type = 0;
	m_windowResized = false;
}


SDL_Window* Window::GetSDLWindow() const
{
	return m_window;
}
