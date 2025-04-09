#pragma once

#if defined(_WIN32)
	#include <windows.h>

	inline void
	raise_thread_priority()
	{
		if( !SetThreadPriority(
				GetCurrentThread(),
				THREAD_PRIORITY_ABOVE_NORMAL) )
			throw std::runtime_error{ "SetThreadPriority failed" };
	}
#else

	inline void
	raise_thread_priority()
	{
		// Not implemented yet.
	}

#endif

