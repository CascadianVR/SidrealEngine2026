#pragma once
#include <iostream>
#include <windows.h>

namespace Logger
{
	// Internal helper for setting console text color
	inline void SetConsoleStyle(const int color)
	{
		const HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
	}

	// Generic logging function with variadic arguments
	template<typename... Args>
	void Info(Args&&... args)
	{
		SetConsoleStyle(7); // Gray (default)
		std::cout << "[INFO]: ";
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
		SetConsoleStyle(7);
	}

	template<typename... Args>
	void Warn(Args&&... args)
	{
		SetConsoleStyle(6); // Yellow
		std::cout << "[WARN]: ";
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
		SetConsoleStyle(7);
	}

	template<typename... Args>
	void Error(Args&&... args)
	{
		SetConsoleStyle(12); // Red
		std::cerr << "[ERROR]: ";
		(std::cerr << ... << std::forward<Args>(args)) << std::endl;
		SetConsoleStyle(7);
	}

	template<typename... Args>
	void Success(Args&&... args)
	{
		SetConsoleStyle(2); // Green
		std::cerr << "[SUCCESS]: ";
		(std::cerr << ... << std::forward<Args>(args)) << std::endl;
		SetConsoleStyle(7);
	}
}
