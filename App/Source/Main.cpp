#include "Application.h"
#include "Rendering/Loader.h"

int main()
{
	Application::Initialize();

	Loader::LoadGLB("Resources/Models/Cascadia.glb");

	// Main loop
	while (Application::IsRunning())
	{
		Application::Update();
	}
	
	return  0;
}