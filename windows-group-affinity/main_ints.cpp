#include "do_work.hpp"

#include <syncstream>

int main(int argc, char ** argv)
{
	try
	{
		std::cout << "version for int" << std::endl;
		windows_group_affinity::do_work<int>(argc, argv);
	}
	catch(const std::exception & x)
	{
		std::osyncstream{ std::cout }
				<< "main: exception caught: " << x.what();
	}

	return 0;
}
