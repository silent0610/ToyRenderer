import std;
import <stdexcept>;
import RendererMod;

int main(int argc, char* argv[])
{

	bool enableValidation{ false };
	if (argc == 1)
	{
	}
	else if (argc == 2)
	{
		if (std::string(argv[1]) == "v")
		{
			std::cout << "need validation\n";
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