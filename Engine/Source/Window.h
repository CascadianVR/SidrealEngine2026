#pragma once
#include <GLFW/glfw3.h>

struct GLFWwindow;

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
	bool ShouldClose() const;
	bool WasResized() const;
	unsigned int GetWidth() const { return m_width; }
	unsigned int GetHeight() const { return m_height; }
	GLFWwindow* GetGlfwWindow() const;
private:
	bool m_windowResized = false;
	int m_height = 0;
	int m_width = 0;
	
	GLFWwindow* m_window = nullptr;
	const char* m_title = nullptr;
};