#include "do_work.hpp"

int main(int argc, char ** argv)
{
	try
	{
		std::cout << "version for double" << std::endl;
		windows_affinity::do_work<double>(argc, argv);
	}
	catch(const std::exception & x)
	{
		std::cout << "main: exception caught: " << x.what();
	}

	return 0;
}
