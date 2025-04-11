#include "run_params.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <string>

namespace run_params
{

namespace
{

[[nodiscard]]
args_parsing_result_t
try_parse_cmd_line_args( int argc, char ** argv )
{
	using namespace std::string_view_literals;

	constexpr std::string_view just_pin{ "pin" };
	constexpr std::string_view pin_prefix{ "pin:" };

	args_parsing_result_t result{ help_requested_t{} };
	run_params_t run_params;

	for( int i = 1; i < argc; ++i )
	{
		//FIXME: реализовать!
		const std::string_view current{ argv[ i ] };
		if( current == "-h"sv || current == "--help"sv )
		{
			// Нет смысла продолжать.
			return result;
		}
		else if( just_pin == current )
		{
			// Нужен самый простой режим пиннинга, без наворотов.
			run_params._pinning = seq_pinning_t{};
		}
		else if( current.starts_with( pin_prefix ) )
		{
			//FIXME: реализовать обработку pinning-а.
		}
		else
		{
			// Возможно, это количество тредов.
			run_params._threads_count = static_cast< unsigned >(
					std::stoul( std::string{ current } ) );
		}
	}

	result = std::move(run_params);

	return result;
}

/// Специальный визитор для проверки корректности результата
/// разбора аргументов командной строки.
struct args_checker_visitor_t
{
	void
	operator()( const help_requested_t & ) const
	{
		// Все нормально, ничего не нужно делать.
	}

	void
	operator()( const run_params_t & params ) const
	{
		// Количество рабочих нитей может быть нулевым только
		// если заданы конкретные ядра, к которым нужна привязка.
		if( !params._threads_count || 0 == params._threads_count.value() )
		{
			if( !std::holds_alternative< selective_pinning_t >(
					params._pinning ) )
			{
				throw std::runtime_error{ "thread count has to be specified" };
			}
		}
	}
};

/// Проверить корректность аргументов.
///
/// Бросает исключение в случае ошибки.
void
ensure_valid_params( const args_parsing_result_t & params )
{
	std::visit( args_checker_visitor_t{}, params );
}

} /* namespace anonymous */

/// Разобрать коммандную строку и получить параметры для работы.
[[nodiscard]]
args_parsing_result_t
parse_cmd_line_args( int argc, char ** argv )
{
	const auto parsing_result = try_parse_cmd_line_args( argc, argv );
	ensure_valid_params( parsing_result );
	return parsing_result;
}

} /* namespace run_params */

