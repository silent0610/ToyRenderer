
import std;
import <stdexcept>;
import RendererMod;
import ConfigMod;
import ToolMod;
int main(int argc, char* argv[])
{
	Config* config = new Config{ Tool::GetProjectPath() + "/Config.json" };

	if (argc == 1)
	{
	}
	else if (argc == 2)
	{
		if (std::string(argv[1]) == "v")
		{

			config->enableValidation = true;
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

	if (config->enableValidation)std::cout << "enable validation\n";
	Renderer renderer{ config };
	try
	{
		renderer.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return  EXIT_SUCCESS;
	delete config;
}