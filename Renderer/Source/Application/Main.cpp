
import std;
import <stdexcept>;
import <format>; // C++20 std::format
import Logger;
import RendererMod; // OLD RENDERER - DISABLED
// import NewRenderer; // NEW RENDERER - ENABLED
import ConfigMod;
import ToolMod;
import DatasetBench;
int main(int argc, char *argv[])
{

	Config *config = new Config{Tool::GetProjectPath() + "/Config.json5"};

	if (DatasetBench::RequestsBatch(argc, argv))
	{
		const auto options = DatasetBench::ParseArgs(argc, argv);
		if (!options)
		{
			delete config;
			return EXIT_FAILURE;
		}
		for (int i = 1; i < argc; ++i)
		{
			if (std::string(argv[i]) == "v")
			{
				config->enableValidation = false;
			}
		}
		DatasetBench::ApplyToConfig(*config, *options);
		Renderer renderer{config};
		if (options->qualityQuota)
		{
			renderer.RunQuotaQualityBench(*options);
		}
		else
		{
			renderer.RunDatasetBench(*options);
		}
		delete config;
		return EXIT_SUCCESS;
	}

	if (argc == 1)
	{
	}
	else if (argc == 2)
	{
		if (std::string(argv[1]) == "v")
		{
			config->enableValidation = false; // Temporarily disable validation for voxelization testing
		}
		else
		{
			std::cerr << "not support arg\n";
		}
	}
	else
	{
		std::cerr << "not support arg\n";
	}

	if (config->enableValidation)
		std::cout << "enable validation\n";

	//std::cout << "=== USING NEW RENDERER ARCHITECTURE ===" << std::endl;

	// Old renderer disabled during refactor testing
	Renderer oldRenderer{config};
	oldRenderer.Run();

	//// Create NewRenderer (simplified for demonstration)
	// NewRenderer newRenderer{ nullptr }; // Window handle will be created internally

	// try
	//{
	//	// Initialize the new renderer
	//	newRenderer.Initialize();

	//	// Run the new renderer
	//	newRenderer.Run();

	//	// Shutdown
	//	newRenderer.Shutdown();
	//}
	// catch (const std::exception& e)
	//{
	//	std::cerr << "NewRenderer error: " << e.what() << std::endl;
	//	return EXIT_FAILURE;
	//}

	delete config;
	return EXIT_SUCCESS;
}