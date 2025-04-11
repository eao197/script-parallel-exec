#pragma once

#include "script.hpp"

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
							var_name,
							1'000'000'000),
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

