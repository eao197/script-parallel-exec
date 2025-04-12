#pragma once

#define NOMINMAX
#include <windows.h>

#include "../templated-script/script.hpp"
#include "../templated-script/demo_script.hpp"

#include "run_params.hpp"

#include <chrono>
#include <format>
#include <iomanip>
#include <syncstream>
#include <mutex>
#include <condition_variable>

namespace windows_affinity
{

namespace impl
{

/// Признак того, должна ли рабочая нить выполнять свою работу
/// в нормальном режиме.
enum class wakeup_type_t : int
{
	/// Рабочая нить должна дождаться сигнала о том, нужно ли ей
	/// работать или нет. Пока такого сигнала еще нет.
	standby,
	/// Рабочая нить должна выполнить свою нормальную работу.
	normal,
	/// Рабочая нить должна сразу же завершиться без выполнения
	/// реальной работы.
	should_shutdown
};

/// Тип объекта для синхронизации старта рабочих нитей.
class startup_sync_t
{
	/// Замок объекта.
	std::mutex _lock;

	/// Условная переменная для ожидания сигнала о начале работы.
	std::condition_variable _waiting_cv;

	/// Индикатор того, как прошел запуск всех рабочих нитей.
	///
	/// Получит значение wakeup_type_t::normal только внутри
	/// метода wakeup_controller_t::wakeup_threads.
	wakeup_type_t _wakeup_type{ wakeup_type_t::standby };

public:
	/// Тип вспомогательного объекта, который в своем деструкторе
	/// дает рабочим нитям сигнал на пробуждение.
	///
	/// Этот сигнал не может быть отдан в деструкторе самого
	/// startup_sync_t, т.к. рабочие нити должны у себя держать
	/// валидную ссылку на startup_sync_t. А когда запускается
	/// деструктор, эта ссылка перестает быть валидной.
	class wakeup_controller_t
	{
		/// Кто реально занимается синхронизацией.
		startup_sync_t & _parent;

		/// Какой сигнал нужно отправить.
		///
		/// По умолчанию отправляем сигнал на аварийное завершение.
		wakeup_type_t _signal_to_use{ wakeup_type_t::should_shutdown };

	public:
		wakeup_controller_t( startup_sync_t & parent )
			: _parent{ parent }
		{}

		~wakeup_controller_t()
		{
			std::lock_guard lock{ _parent._lock };
			_parent._wakeup_type = _signal_to_use;
			_parent._waiting_cv.notify_all();
		}

		/// Индикатор того, что запуск рабочих нитей прошел нормально.
		///
		/// Дает команду рабочим нитям проснуться.
		void
		wakeup_threads()
		{
			std::lock_guard lock{ _parent._lock };

			_parent._wakeup_type = wakeup_type_t::normal;
			_parent._waiting_cv.notify_all();
		}
	};

	startup_sync_t() = default;

	/// Ожидание возможности стартовать.
	[[nodiscard]] wakeup_type_t
	arrive_and_wait()
	{
		std::unique_lock lock{ _lock };

		if( wakeup_type_t::standby == _wakeup_type )
			// Слишком рано, нужно подождать.
			_waiting_cv.wait( lock,
					[this]{ return wakeup_type_t::standby != _wakeup_type; } );

		return _wakeup_type;
	}
};

void
pin_to_core(
	const run_params::thread_pinning_info_t & pinning_info )
{
	PROCESSOR_NUMBER ideal_processor{
			.Group = static_cast<WORD>(pinning_info._group),
			.Number = static_cast<BYTE>(pinning_info._processor)
	};
	//FIXME: действительно ли нужно забирать предыдущий идеальный процессор?
	//Пока забираем, вдруг это даст какой-то полезной информации для размышлений.
	PROCESSOR_NUMBER current_ideal_processor;
	if( !SetThreadIdealProcessorEx(
			GetCurrentThread(),
			std::addressof(ideal_processor),
			std::addressof(current_ideal_processor) ) )
	{
		throw std::runtime_error{
			std::format(
					"SetThreadIdealProcessorEx with Group={} and Number={} "
					"failed (GetLastError={})",
					pinning_info._group,
					static_cast<unsigned short>(pinning_info._processor),
					GetLastError() )
			};
	}
	else
	{
		std::osyncstream(std::cout)
				<< "  old ideal processor was: " << current_ideal_processor.Group
				<< '-' << static_cast<unsigned short>(current_ideal_processor.Number)
				<< std::endl;
	}
}

template< typename T >
void
exec_demo_script_thread_body(
	/// Куда нужно привязывать нить. Если core_index пуст, то
	/// привязки нити к ядру не выполняется.
	std::optional<run_params::thread_pinning_info_t> core_index,
	/// Для синхронизации момента старта.
	startup_sync_t & start_latch,
	const script::statement_shptr_t<T> & stm,
	std::chrono::steady_clock::duration & time_receiver)
{
	try
	{
		if( core_index.has_value() )
			pin_to_core( *core_index );

		const auto wakeup_type = start_latch.arrive_and_wait();
		if( wakeup_type_t::should_shutdown == wakeup_type )
		{
			// Работать нельзя и нужно быстро завершить свои действия.
			return;
		}

		// Раз оказались здесь, значит можно работать в нормальном режиме.
		const auto started_at = std::chrono::steady_clock::now();
		script::execute(stm);
		const auto finished_at = std::chrono::steady_clock::now();

		time_receiver = finished_at - started_at;
	}
	catch( const std::exception & x)
	{
		std::osyncstream{ std::cerr }
				<< "exec_demo_script_thread_body: exception caught: "
				<< x.what() << std::endl;
	}
}

/// Сбор и печать доступной информации о системе.
void
collect_and_report_some_system_info()
{
	std::osyncstream cout{ std::cout };

	cout << "some system related information:\n" << std::flush;

	// Определяем сколько всего логических процессоров в системе.
	SYSTEM_INFO sys_info;
	GetSystemInfo( &sys_info );
	cout << "  GetSystemInfo: dwNumberOfProcessors: "
			<< sys_info.dwNumberOfProcessors << std::endl;
	cout << "  GetSystemInfo: dwActiveProcessorMask: "
			<< std::hex << sys_info.dwActiveProcessorMask << std::dec
			<< std::endl;

	cout << "  ---\n";

	cout << "  std::thread::hardware_concurrency: "
			<< std::thread::hardware_concurrency() << std::endl;

	cout << "  ---\n";

	cout << "  GetActiveProcessorCount(ALL_PROCESSOR_GROUPS): "
			<< GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) << std::endl;
	cout << "  GetActiveProcessorCount(0): "
			<< GetActiveProcessorCount(0) << std::endl;
	cout << "  GetActiveProcessorGroupCount: "
			<< GetActiveProcessorGroupCount() << std::endl;

	cout << "  ---\n";

	// Что там с affinity для всего процесса?
	{
		DWORD_PTR process_affinity, system_affinity;
		if( !GetProcessAffinityMask( GetCurrentProcess(),
				&process_affinity,
				&system_affinity ) )
			throw std::runtime_error{ "GetProcessAffinityMask failed" };

		cout << "  process affinity: " << std::hex << process_affinity
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
		std::optional< run_params::thread_pinning_info_t >
		current_index() const = 0;

		virtual void
		advance() = 0;
	};

	/// Реализация для случая, когда привязка вообще не нужна.
	class no_pinning_selector_t final : public abstract_selector_t
	{
	public:
		std::optional< run_params::thread_pinning_info_t >
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
		/// Текущая группа процессоров.
		///
		/// Исходим из того, что группа с номером 0 всегда существует.
		WORD _current_group{ 0 };

		/// Общее количество активных групп.
		const WORD _total_groups;

		/// Общее количество процессоров в текущей группе.
		WORD _processors_in_current_group;

		/// Текущий процессор в текущей группе.
		///
		/// Исходим из того, что процессоры в группе нумеруются с нуля.
		WORD _current_processor{ 0 };

		[[nodiscard]] static
		WORD
		total_groups_count()
		{
			const WORD total_groups = GetActiveProcessorGroupCount();
			if( !total_groups )
				throw std::runtime_error{
						std::format(
								"unable to detect processor group count "
								"(GetLastError={})",
								GetLastError() )
					};
			return total_groups;
		}

		[[nodiscard]] static
		WORD
		how_many_processors_in_group( WORD group_index )
		{
			const WORD processors = static_cast<WORD>(
					GetActiveProcessorCount( group_index ) );
			if( !processors )
				throw std::runtime_error{
						std::format(
								"unable to detect processor count for group {} "
								"(GetLastError={})",
								group_index,
								GetLastError() )
					};
			return processors;
		}

	public:
		seq_selector_t( const run_params::seq_pinning_t & /*params*/ )
			: _total_groups{ total_groups_count() }
			, _processors_in_current_group{
					how_many_processors_in_group( _current_group ) }
		{
			std::osyncstream{ std::cout }
					<< "starting from group " << _current_group
					<< " with " << _processors_in_current_group
					<< " processor(s)" << std::endl;
		}

		std::optional< run_params::thread_pinning_info_t >
		current_index() const override
		{
			return {
					run_params::thread_pinning_info_t{
						_current_group,
						_current_processor
					}
				};
		}

		void
		advance() override
		{
			while( _current_group < _total_groups )
			{
				++_current_processor;

				if( _current_processor < _processors_in_current_group )
				{
					// Можно брать следующий процессор в текущей группе.
					return;
				}

				// В противном случае текущая группа закончилась и нужно
				// пробовать перейти на следующую.
				++_current_group;
				if( _current_group < _total_groups )
				{
					_current_processor = 0;
					_processors_in_current_group = how_many_processors_in_group(
							_current_group );

					std::osyncstream{ std::cout }
							<< "switching to the next processor group ("
							<< _current_group << " of " << _total_groups
							<< "), processors in this group: "
							<< _processors_in_current_group << std::endl;

					if( _current_processor < _processors_in_current_group )
					{
						// Текущая группа не пуста, так что начинаем ее
						// использовать с самого первого процессора.
						return;
					}
				}
			}

			throw std::runtime_error{
					std::format(
							"no more processor groups available "
							"(total groups: {})",
							_total_groups )
				};
		}
	};

	/// Реализация для случая, когда нужно использовать указанные ядра.
	class selected_selector_t final : public abstract_selector_t
	{
		const std::vector< run_params::thread_pinning_info_t > _cores;
		std::size_t _index_in_cores{};

	public:
		selected_selector_t( const run_params::selective_pinning_t & params )
			: _cores{ params._cores }
		{}

		std::optional< run_params::thread_pinning_info_t >
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
			std::osyncstream{ std::cout }
					<< "no pinning will be used" << std::endl;
			return std::make_unique< no_pinning_selector_t >();
		}

		[[nodiscard]] std::unique_ptr< abstract_selector_t >
		operator()( const run_params::seq_pinning_t & params ) const
		{
			std::osyncstream{ std::cout }
					<< "simple sequential pinning will be used"
					<< std::endl;
			return std::make_unique< seq_selector_t >( params );
		}

		[[nodiscard]] std::unique_ptr< abstract_selector_t >
		operator()( const run_params::selective_pinning_t & params ) const
		{
			std::osyncstream{ std::cout }
					<< "pinning to selected cores will be used" << std::endl;
			return std::make_unique< selected_selector_t >( params );
		}
	};
public:
	core_index_selector_t( const run_params::pinning_params_t & params )
		: _selector{ std::visit( selector_maker_t{}, params ) }
	{
	}

	[[nodiscard]]
	std::optional< run_params::thread_pinning_info_t >
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
	std::osyncstream{ std::cout }
			<< "thread(s) to be used: " << threads_count << std::endl;

	// Сам демо-скрипт для выполнения.
	const auto demo_script = make_demo_script<T>();

	// Вспомогательный объект для вычисления ядер, к которым может
	// потребоваться привязка рабочих нитей.
	core_index_selector_t cores_selector{ params._pinning };

	// Очень важно, чтобы данный объект закончил свою жизнь уже
	// после того, как все рабочие нити будут уничтожены.
	startup_sync_t start_latch;

	// Приемник итогового времени работы каждой из рабочих нитей.
	std::vector< std::chrono::steady_clock::duration > times{
			threads_count,
			std::chrono::steady_clock::duration::zero()
	};

	// Создаем и запускаем рабочие нити.
	std::vector< std::jthread > threads;
	threads.reserve(threads_count);

	// Очень важно, чтобы этот объект закончил свою жизнь
	// до того, как threads будет разрушен. Это нужно для того,
	// чтобы при преждевременном выходе из функции startup_sync_t
	// дал сигнал на завершение рабочих нитей.
	startup_sync_t::wakeup_controller_t wakeup_controller{ start_latch };

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
			std::osyncstream{ std::cout }
					<< "starting worker #" << (i+1)
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

	// Рабочие нити запущены, можно дать им сигнал на начало работы.
	std::osyncstream{ std::cout }
			<< "sending `start` signal to worker threads"
			<< std::endl;
	wakeup_controller.wakeup_threads();

	// Ждем пока все завершиться.
	for( auto & thr : threads )
	{
		thr.join();
	}

	// Осталось распечатать результаты.
	for( const auto & d : times )
	{
		const double as_seconds = std::chrono::duration_cast<
				std::chrono::milliseconds >(d).count() / 1000.0;
		std::osyncstream{ std::cout }
				<< std::setprecision(4) << as_seconds << std::endl;
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
		std::osyncstream{ std::cout } << "Usage:\n\t"
			<< _argv_0
			<< " [thread_count] [pin[:<core-index(es)>]]\n\n"
			<< "where `pin` can be in one of the following formats:\n\n"
				"pin             pin threads to logical processes sequentially\n"
				"                starting from 0-0\n"
				"pin:I,J,K[,..]  pin thread only to specified logical processes\n"
				"                For example: pin:0-1,0-2,1-3,1-4\n"
				"\n"
				"NOTE: `thread_count` is optional only if `pin` with enumeration\n"
				"of logical processors is used. It means that:\n\n"
			<< "\t" << _argv_0 << " pin:0-0,0-2,0-4\n\n"
			<< "is OK, but:\n\n"
			<< "\t" << _argv_0 << " pin\n\n"
			<< "is an error, it has to be:\n\n"
			<< "\t" << _argv_0 << " 10 pin"
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

