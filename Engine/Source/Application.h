#pragma once

#include <memory>
#include <chrono>
#include "Window.h"

class Application {
public:
	static void Initialize();
	static void Update();
	static bool IsRunning() { return !m_window->ShouldClose(); }

	static Window* GetWindow() { return m_window.get(); }
	static float GetDeltaTime() { return m_deltaTime; }
	static double GetElapsedTime() { return m_elapsedTime; }

private:
	static inline std::unique_ptr<Window> m_window;
	static inline std::chrono::system_clock::time_point m_lastTime;
	static inline float m_deltaTime;
	static inline double m_elapsedTime;

};