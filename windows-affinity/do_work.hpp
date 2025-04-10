#pragma once

#define NOMINMAX
#include <windows.h>

#include "../templated-script/script.hpp"
#include "../templated-script/demo_script.hpp"

#include <chrono>
#include <iomanip>
#include <latch>

inline void
raise_thread_priority()
{
	if( !SetThreadPriority(
			GetCurrentThread(),
			THREAD_PRIORITY_TIME_CRITICAL) )
		throw std::runtime_error{ "SetThreadPriority failed" };
}

template< typename T >
void
exec_demo_script_thread_body(
	DWORD_PTR thread_affinity_mask,
	std::latch & start_latch,
	const script::statement_shptr_t<T> & stm,
	std::chrono::steady_clock::duration & time_receiver)
{
	try
	{
		// Привязываем себя к конкретному ядру.
		if( auto old_mask = SetThreadAffinityMask( GetCurrentThread(),
				thread_affinity_mask );
				0 == old_mask )
		{
			throw std::runtime_error{ "SetThreadAffinityMask failed" };
		}

		// Повышаем свой приоритет.
		raise_thread_priority();

		start_latch.arrive_and_wait();

		const auto started_at = std::chrono::steady_clock::now();
		script::execute(stm);
		const auto finished_at = std::chrono::steady_clock::now();

		time_receiver = finished_at - started_at;
	}
	catch( const std::exception & x)
	{
		std::cerr << "exec_demo_script_thread_body: exception caught: "
				<< x.what() << std::endl;
	}
}

class processor_index_enumerator_t
{
	const DWORD_PTR _processors_mask;
	DWORD_PTR _current_index{ 0 };

public:
	processor_index_enumerator_t(
		DWORD_PTR processors_mask )
		: _processors_mask{ processors_mask }
	{}

	void
	try_find_index()
	{
		if( !_current_index )
			_current_index = 1;
		else
			_current_index += 1;

		bool found = false;
		while( !found && (_current_index != 0) )
		{
			found = 0 != (_processors_mask & _current_index);
			if( !found )
				// По текущему индексу процессора нет, пытаемся
				// попробовать следующий.
				++_current_index;
		}

		if( !found )
			throw std::runtime_error{ "no more indexes of logical processors" };
	}

	auto
	current_index() const
	{
		if( !_current_index )
			throw std::runtime_error{ "no more indexes of logical processors" };
		return _current_index;
	}
};

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

	// Определяем сколько всего логических процессоров в системе.
	SYSTEM_INFO sys_info;
	GetSystemInfo( &sys_info );
	std::cout << "total logical processors: "
			<< sys_info.dwNumberOfProcessors << std::endl;
	if( threads_count > sys_info.dwNumberOfProcessors )
		threads_count = sys_info.dwNumberOfProcessors;

	// Что там с affinity для всего процесса?
	{
		DWORD_PTR process_affinity, system_affinity;
		if( !GetProcessAffinityMask( GetCurrentProcess(),
				&process_affinity,
				&system_affinity ) )
			throw std::runtime_error{ "GetProcessAffinityMask failed" };

		std::cout << "process affinity: " << std::hex << process_affinity
				<< ", system affinity: " << system_affinity << std::dec
				<< std::endl;
	}

	std::cout << "thread(s) to be used: " << threads_count << std::endl;

	// Поскольку знаем сколько всего будет нитей, то можем сразу создать
	// барьер для синхронизации старта.
	std::latch start_latch{ static_cast<ptrdiff_t>(threads_count) };

	// Создаем и запускаем рабочие нити.
	std::vector< std::jthread > threads;
	threads.reserve(threads_count);

	std::vector< std::chrono::steady_clock::duration > times{
			threads_count,
			std::chrono::steady_clock::duration::zero()
	};

	// Нам нужно будет искать номера актуальных процессоров.
	processor_index_enumerator_t processor_index_finder{
			sys_info.dwActiveProcessorMask
	};

	// Сам демо-скрипт для выполнения.
	const auto demo_script = make_demo_script<T>();

	// Непосредственный запуск рабочих нитей.
	for( std::size_t i = 0; i != threads_count; ++i )
	{
		// NOTE: если индекс очередного процессора не будет найден,
		// то вылетит исключение.
		processor_index_finder.try_find_index();
		std::cout << "starting worker #" << (i+1)
				<< " on processor "
				<< processor_index_finder.current_index()
				<< std::endl;

		threads.push_back(
			std::jthread{
				exec_demo_script_thread_body<T>,
				processor_index_finder.current_index(),
				std::ref(start_latch),
				std::cref(demo_script),
				std::ref(times[i])
			}
		);
	}

	// Ждем пока все завершиться.
	for( auto & thr : threads )
		thr.join();

	// Осталось распечатать результаты.
	for( const auto & d : times )
	{
		const double as_seconds = std::chrono::duration_cast<
				std::chrono::milliseconds >(d).count() / 1000.0;
		std::cout << std::setprecision(4) << as_seconds << std::endl;
	}
}

