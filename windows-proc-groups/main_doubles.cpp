#include "do_work.hpp"

#include <syncstream>

int main(int argc, char ** argv)
{
	try
	{
		std::cout << "version for double" << std::endl;
		windows_proc_groups::do_work<double>(argc, argv);
	}
	catch(const std::exception & x)
	{
		std::osyncstream{ std::cerr }
				<< "main: exception caught: " << x.what();
	}

	return 0;
}
