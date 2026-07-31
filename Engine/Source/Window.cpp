#include "Window.h"
#include "Logger.h"

#include <GLFW/glfw3.h>
#include <stdexcept>

Window::Window(const int width, const int height, const char* title)
{
	m_width = width;
	m_height = height;
	m_title = title;
	m_windowResized = false;

	if (!glfwInit())
	{
		throw std::runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (!m_window)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	// Set the user pointer to this instance of Window
	glfwSetWindowUserPointer(m_window, this);

	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height)
	{
		if (const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) 
		{
			self->m_windowResized = true;
			self->m_height = height;
			self->m_width = width;
		}
	});

	Logger::Success("Created Window");
}

Window::~Window()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Window::ShouldClose() const
{
	return glfwWindowShouldClose(m_window);
}

bool Window::WasResized() const
{
	return m_windowResized;
}

void Window::Show() const
{
	glfwShowWindow(m_window);
}

void Window::Hide() const
{
	glfwHideWindow(m_window);
}

void Window::PollEvents()
{
	m_windowResized = false;
	glfwPollEvents();
}

GLFWwindow* Window::GetGlfwWindow() const
{
	return m_window;
}
