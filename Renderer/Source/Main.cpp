
import std;
import <stdexcept>;
import RendererMod;
import ConfigMod;

int main(int argc, char* argv[])
{

	Config config{ "Config.json" };
	bool enableValidation{ config.enableValidation };

	if (argc == 1)
	{
	}
	else if (argc == 2)
	{
		if (std::string(argv[1]) == "v")
		{

			enableValidation = true;
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
	if (enableValidation)std::cout << "enable validation\n";
	Renderer renderer{ enableValidation };
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
}