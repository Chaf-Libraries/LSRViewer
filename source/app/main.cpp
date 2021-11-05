#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif

#include <vulkanexamplebase.h>

#include <scene/scene_loader.h>
#include <scene/components/transform.h>
#include <scene/components/camera.h>
#include <scene/components/mesh.h>

#include <scene/cacher/lru.h>

#include <app/app.h>

Application* app;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (app != NULL)
	{
		app->handleMessages(hWnd, uMsg, wParam, lParam);
	}

	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	{
		for (int32_t i = 0; i < __argc; i++)
		{
			Application::args.push_back(__argv[i]);
		};
		app = new Application();

		app->initVulkan();
		app->setupWindow(hInstance, WndProc);
		app->prepare();
		app->renderLoop();
		delete app;
		std::cin.get();
	}
	return 0;
}