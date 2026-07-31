#include "Application.h"
#include "Rendering/Vulkan/VulkanCore.h"

void Application::Initialize()
{
	m_lastTime = std::chrono::system_clock::now();
	m_window = std::make_unique<Window>(1280, 720, "Vulkan Window");
	m_window->Show();
	VulkanCore::Initialize(m_window.get());
}

void Application::Update()
{
	// Time and delta time
	const auto currentTime = std::chrono::system_clock::now();
	const std::chrono::duration<float> elapsed = currentTime - m_lastTime;
	m_deltaTime = elapsed.count();
	m_lastTime = currentTime;
	m_elapsedTime += m_deltaTime;

	m_window->PollEvents();
	VulkanCore::Render();
	m_window->ClearEvents();
}
