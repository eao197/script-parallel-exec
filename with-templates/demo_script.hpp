#pragma once

#include "script.hpp"
#include "raise_thread_priority.hpp"

#include <chrono>
#include <iomanip>

template< typename T >
[[nodiscard]] script::statement_shptr_t<T>
make_demo_script()
{
	static const std::string var_name{ "j" };

	std::vector< script::statement_shptr_t<T> > statements;


	statements.push_back(
			std::make_shared< script::statements::assign_to_t<T> >(
					var_name, 0));
	statements.push_back(
			std::make_shared< script::statements::while_loop_t<T> >(
					std::make_shared< script::expressions::less_than_t<T> >(
							var_name, 500'000'000),
					std::make_shared< script::statements::increment_by_t<T> >(
							var_name, 1)
			)
	);
	statements.push_back(
			std::make_shared< script::statements::print_value_t<T> >(
					var_name));

	return std::make_shared< script::statements::compound_stmt_t<T> >(
			std::move(statements));
}

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

