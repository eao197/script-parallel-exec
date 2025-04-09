#include "demo_script.hpp"

int main(int argc, char ** argv)
{
	try
	{
		std::cout << "version for int" << std::endl;
		do_work<int>(argc, argv);
	}
	catch(const std::exception & x)
	{
		std::cout << "main: exception caught: " << x.what();
	}

	return 0;
}
