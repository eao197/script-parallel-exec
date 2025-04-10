#pragma once

#include "../templated-script/script.hpp"
#include "../templated-script/demo_script.hpp"

#include "raise_thread_priority.hpp"

#include <chrono>
#include <iomanip>

template< typename T >
void
exec_demo_script_thread_body(
	const script::statement_shptr_t<T> & stm,
	std::chrono::steady_clock::duration & time_receiver)
{
	raise_thread_priority();

	const auto started_at = std::chrono::steady_clock::now();
	script::execute(stm);
	const auto finished_at = std::chrono::steady_clock::now();

	time_receiver = finished_at - started_at;
}

template< typename T >
void
do_work(int argc, char ** argv)
{
	std::size_t threads_count{ 4 };
	if( 2 == argc )
	{
		threads_count = std::stoul(argv[1]);
		if( 0 == threads_count )
			throw std::runtime_error{ "number of threads can't 0" };
	}

	std::cout << "thread(s) to be used: " << threads_count << std::endl;

	std::vector< std::jthread > threads;
	threads.reserve(threads_count);

	std::vector< std::chrono::steady_clock::duration > times{
			threads_count,
			std::chrono::steady_clock::duration::zero()
	};

	const auto demo_script = make_demo_script<T>();

	for( std::size_t i = 0; i != threads_count; ++i )
	{
		threads.push_back(
			std::jthread{
				exec_demo_script_thread_body<T>,
				std::cref(demo_script),
				std::ref(times[i])
			}
		);
	}

	for( auto & thr : threads )
		thr.join();

	for( const auto & d : times )
	{
		const double as_seconds = std::chrono::duration_cast<
				std::chrono::milliseconds >(d).count() / 1000.0;
		std::cout << std::setprecision(4) << as_seconds << std::endl;
	}
}

