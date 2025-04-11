#pragma once

#define NOMINMAX
#include <windows.h>

#include "../templated-script/script.hpp"
#include "../templated-script/demo_script.hpp"

#include "run_params.hpp"

#include <chrono>
#include <format>
#include <iomanip>
#include <latch>

namespace windows_affinity
{

namespace impl
{

void
pin_to_core(
	run_params::core_index_t core_index)
{
	const DWORD_PTR thread_affinity_mask = (DWORD_PTR{1} << core_index);
	
	// Привязываем себя к конкретному ядру.
	if( auto old_mask = SetThreadAffinityMask( GetCurrentThread(),
			thread_affinity_mask );
			0 == old_mask )
	{
		throw std::runtime_error{
				std::format( "SetThreadAffinityMask failed, core_index={}",
						core_index )
			};
	}
}

template< typename T >
void
exec_demo_script_thread_body(
	/// Куда нужно привязывать нить. Если core_index пуст, то
	/// привязки нити к ядру не выполняется.
	std::optional<run_params::core_index_t> core_index,
	std::latch & start_latch,
	const script::statement_shptr_t<T> & stm,
	std::chrono::steady_clock::duration & time_receiver)
{
	try
	{
		if( core_index.has_value() )
			pin_to_core( *core_index );

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

/// Сбор и печать доступной информации о системе.
void
collect_and_report_some_system_info()
{
	std::cout << "some system related information:\n" << std::flush;

	// Определяем сколько всего логических процессоров в системе.
	SYSTEM_INFO sys_info;
	GetSystemInfo( &sys_info );
	std::cout << "  GetSystemInfo: dwNumberOfProcessors: "
			<< sys_info.dwNumberOfProcessors << std::endl;
	std::cout << "  GetSystemInfo: dwActiveProcessorMask: "
			<< std::hex << sys_info.dwActiveProcessorMask << std::dec
			<< std::endl;

	std::cout << "  ---\n";

	std::cout << "  std::thread::hardware_concurrency: "
			<< std::thread::hardware_concurrency() << std::endl;

	std::cout << "  ---\n";

	std::cout << "  GetActiveProcessorCount(ALL_PROCESSOR_GROUPS): "
			<< GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) << std::endl;
	std::cout << "  GetActiveProcessorCount(0): "
			<< GetActiveProcessorCount(0) << std::endl;
	std::cout << "  GetActiveProcessorGroupCount: "
			<< GetActiveProcessorGroupCount() << std::endl;

	std::cout << "  ---\n";

	// Что там с affinity для всего процесса?
	{
		DWORD_PTR process_affinity, system_affinity;
		if( !GetProcessAffinityMask( GetCurrentProcess(),
				&process_affinity,
				&system_affinity ) )
			throw std::runtime_error{ "GetProcessAffinityMask failed" };

		std::cout << "  process affinity: " << std::hex << process_affinity
				<< ", system affinity: " << system_affinity << std::dec
				<< std::endl;
	}
}

[[nodiscard]]
std::size_t
detect_threads_count( const run_params::run_params_t & params )
{
	std::size_t count = params._threads_count.value_or( std::size_t{ 0 } );

	if( const auto * selected_cores =
			std::get_if< run_params::selective_pinning_t >(
					std::addressof(params._pinning) ) )
	{
		// Количество нитей не может превышать количество ядер,
		// которые были явно указаны для привязки.
		// Но если thread_count не был указан вообще, то нужно брать
		// количество перечисленных пользователем ядер.
		if( params._threads_count.has_value() )
			count = std::min( count, selected_cores->_cores.size() );
		else
			count = selected_cores->_cores.size();
	}

	if( !count )
		throw std::runtime_error{ "thread_count can't be 0" };

	return count;
}

/// Вспомогательный класс для вычисления номера следующего
/// ядра для привязки рабочей нити.
class core_index_selector_t
{
	/// Интерфейс объекта, который будет вычислять номер ядра.
	class abstract_selector_t
	{
	public:
		virtual ~abstract_selector_t() = default;

		[[nodiscard]] virtual
		std::optional< run_params::core_index_t >
		current_index() const = 0;

		virtual void
		advance() = 0;
	};

	/// Реализация для случая, когда привязка вообще не нужна.
	class no_pinning_selector_t final : public abstract_selector_t
	{
	public:
		std::optional< run_params::core_index_t >
		current_index() const override
		{
			return std::nullopt;
		}

		void
		advance() override
		{ /* Ничего не нужно делать. */ }
	};

	/// Реализация для случая, когда нужно просто последовательно
	/// привязывать к следующему ядру.
	class seq_selector_t final : public abstract_selector_t
	{
		run_params::core_index_t _current_index;

	public:
		seq_selector_t( const run_params::seq_pinning_t & params )
			: _current_index{ params._start_from }
		{}

		std::optional< run_params::core_index_t >
		current_index() const override
		{
			return { _current_index };
		}

		void
		advance() override
		{
			++_current_index;
		}
	};

	/// Реализация для случая, когда нужно использовать указанные ядра.
	class selected_selector_t final : public abstract_selector_t
	{
		const std::vector< run_params::core_index_t > _cores;
		std::size_t _index_in_cores{};

	public:
		selected_selector_t( const run_params::selective_pinning_t & params )
			: _cores{ params._cores }
		{}

		std::optional< run_params::core_index_t >
		current_index() const override
		{
			return { _cores.at( _index_in_cores ) };
		}

		void
		advance() override
		{
			++_index_in_cores;
		}
	};

	/// Актуальный селектор для вычисления номеров ядер для привязки.
	std::unique_ptr< abstract_selector_t > _selector;

	/// Вспомогательный визитор для создания актуального селектора.
	///
	/// Предназначен для использования совместно с std::visit.
	struct selector_maker_t
	{
		[[nodiscard]] std::unique_ptr< abstract_selector_t >
		operator()( const run_params::no_pinning_t & ) const
		{
			std::cout << "no pinning will be used" << std::endl;
			return std::make_unique< no_pinning_selector_t >();
		}

		[[nodiscard]] std::unique_ptr< abstract_selector_t >
		operator()( const run_params::seq_pinning_t & params ) const
		{
			std::cout << "simple sequential pinning will be used "
					"(starting from: " << params._start_from << ")"
					<< std::endl;
			return std::make_unique< seq_selector_t >( params );
		}

		[[nodiscard]] std::unique_ptr< abstract_selector_t >
		operator()( const run_params::selective_pinning_t & params ) const
		{
			std::cout << "pinning to selected cores will be used" << std::endl;
			return std::make_unique< selected_selector_t >( params );
		}
	};
public:
	core_index_selector_t( const run_params::pinning_params_t & params )
		: _selector{ std::visit( selector_maker_t{}, params ) }
	{
	}

	[[nodiscard]]
	std::optional< run_params::core_index_t >
	current_index() const
	{
		return _selector->current_index();
	}

	void
	advance()
	{
		_selector->advance();
	}
};

/// Выполнение основной работы.
template< typename T >
void
do_main_work( const run_params::run_params_t & params )
{
	collect_and_report_some_system_info();

	// Сколько же нам потребуется нитей?
	const auto threads_count = detect_threads_count( params );
	std::cout << "thread(s) to be used: " << threads_count << std::endl;

	// Сам демо-скрипт для выполнения.
	const auto demo_script = make_demo_script<T>();

	// Вспомогательный объект для вычисления ядер, к которым может
	// потребоваться привязка рабочих нитей.
	core_index_selector_t cores_selector{ params._pinning };

	// Поскольку знаем сколько всего будет нитей, то можем сразу создать
	// барьер для синхронизации старта.
	std::latch start_latch{ static_cast<ptrdiff_t>(threads_count) };

	// Создаем и запускаем рабочие нити.
	std::vector< std::jthread > threads;
	threads.reserve(threads_count);

	// Приемник итогового времени работы каждой из рабочих нитей.
	std::vector< std::chrono::steady_clock::duration > times{
			threads_count,
			std::chrono::steady_clock::duration::zero()
	};

	// Непосредственный запуск рабочих нитей.
	for( std::size_t i = 0; i != threads_count;
			++i,
			cores_selector.advance() )
	{
		// NOTE: если индекс очередного ядра не будет найден,
		// то вылетит исключение.
		const auto core_index = cores_selector.current_index();
		if( core_index.has_value() )
		{
			std::cout << "starting worker #" << (i+1)
					<< " on logical processor "
					<< *core_index
					<< std::endl;
		}

		threads.push_back(
			std::jthread{
				exec_demo_script_thread_body<T>,
				core_index,
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

/// Специальный visitor для обработки результатов парсинга
/// аргументов коммандной строки.
template< typename T >
class cmd_line_args_handler_t
{
	const char * _argv_0;

public:
	explicit cmd_line_args_handler_t( const char * argv_0 )
		: _argv_0{ argv_0 }
	{}

	void
	operator()( const run_params::help_requested_t & ) const
	{
		std::cout << "Usage:\n\t"
			<< _argv_0
			<< " [thread_count] [pin[:<core-index(es)>]]"
			<< std::endl;
	}

	void
	operator()( const run_params::run_params_t & params ) const
	{
		do_main_work<T>( params );
	}
};

} // namespace impl

template< typename T >
void
do_work(int argc, char ** argv)
{
	using namespace impl;

	const auto parsed_args = run_params::parse_cmd_line_args( argc, argv );

	std::visit(
			cmd_line_args_handler_t<T>{ argv[0] },
			parsed_args );
}

} // namespace windows_affinity

