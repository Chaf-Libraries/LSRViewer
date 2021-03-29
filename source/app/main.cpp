#include <app/app.h>

#include <framework/platform/windows/windows_platform.h>

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
	vkb::WindowsPlatform platform(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

	std::unique_ptr<vkb::Application> app = std::make_unique<chaf::App>();
	app->set_name("Large Scene Renderer");

	if (platform.initialize(std::move(app)))
	{
		platform.main_loop();
		platform.terminate(vkb::ExitCode::Success);
	}
	else
	{
		platform.terminate(vkb::ExitCode::UnableToRun);
	}

	return EXIT_SUCCESS;
}