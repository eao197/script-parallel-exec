#include "run_params.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <regex>

namespace run_params
{

namespace
{

[[nodiscard]]
pinning_params_t
try_parse_adv_pinning_mode( std::string arg_value )
{
	using sregex_iterator_t = std::sregex_iterator;
	using smatch_t = std::smatch;
	constexpr auto regex_kind = std::regex::ECMAScript;

	const auto to_pinning_info = [](
			const sregex_iterator_t & it,
			std::size_t capture_index = 1 )
		-> thread_pinning_info_t
	{
		return {
				static_cast< processor_group_id_t >(
						std::stoul( smatch_t{ *it }.str( capture_index ) ) ),
				static_cast< processor_number_t >(
						std::stoul( smatch_t{ *it }.str( capture_index + 1 ) ) )
			};
	};

	const auto make_it = [](
			const std::string & from,
			const std::regex & regex )
	{
		return sregex_iterator_t{ from.begin(), from.end(), regex };
	};

	const sregex_iterator_t not_found{};

	// Обрабатываем перечисление конкретных ядер. Т.е. `pin:0/1` или `pin:0/1,`
	// или `pin:0/1,0/2` или `pin:0/1,0/2,0/4,0/5` и т.д.
	const std::regex one_selected_core{ R"(^(\d+)-(\d+)$)", regex_kind };
	const std::regex selected_core_with_comma{ R"(^(\d+)-(\d+),(.*)$)", regex_kind };

	selective_pinning_t selected;
	while( !arg_value.empty() )
	{
		if( auto it_simple = make_it( arg_value, one_selected_core );
				it_simple != not_found )
		{
			selected._cores.push_back( to_pinning_info( it_simple ) );

			// Продолжать нет мысла.
			arg_value.clear();
		}
		else if( auto it_with_comma =
				make_it( arg_value, selected_core_with_comma );
				it_with_comma != not_found )
		{
			selected._cores.push_back( to_pinning_info( it_with_comma ) );

			// Продолжаем с остатком, если таковой есть.
			arg_value = smatch_t{ *it_with_comma }.str( 3 );
		}
		else
			throw std::runtime_error{
					"unable to parse enumeration of core indexes, problem "
					"with substring: `" + arg_value + "`"
			};
	}

	return { std::move(selected) };
}

[[nodiscard]]
args_parsing_result_t
try_parse_cmd_line_args( int argc, char ** argv )
{
	using namespace std::string_view_literals;

	constexpr std::string_view just_pin{ "pin" };
	constexpr std::string_view pin_prefix{ "pin:" };

	args_parsing_result_t result{ help_requested_t{} };
	if( 1 == argc )
	{
		// Нет смысла продолжать.
		return result;
	}

	run_params_t run_params;

	for( int i = 1; i < argc; ++i )
	{
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
			run_params._pinning = try_parse_adv_pinning_mode(
					std::string{ current.substr( pin_prefix.size() ) } );
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

