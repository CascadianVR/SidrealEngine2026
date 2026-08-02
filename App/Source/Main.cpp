#include "Application.h"

int main()
{
	Application::Initialize();

	// Main loop
	while (Application::IsRunning())
	{
		Application::Update();
	}
	
	return  0;
}