#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
	#include <windows.h>

	void
	raise_thread_priority()
	{
		if( !SetThreadPriority(
				GetCurrentThread(),
				THREAD_PRIORITY_ABOVE_NORMAL) )
			throw std::runtime_error{ "SetThreadPriority failed" };
	}
#else

	void
	raise_thread_priority()
	{
		// Not implemented yet.
	}

#endif

namespace script
{

class exec_context_t
{
	std::unordered_map<std::string, double> _vars;

public:
	exec_context_t() = default;

	void
	assign_to(
		const std::string & name,
		double value)
	{
		_vars[name] = value;
	}

	double &
	get_for_modification(const std::string & name)
	{
		auto it = _vars.find(name);
		if( it == _vars.end() )
			throw std::runtime_error{ "there is no such variable: " + name };

		return it->second;
	}
};

class statement_t : public std::enable_shared_from_this< statement_t >
{
public:
	virtual ~statement_t() = default;

	virtual void
	exec(exec_context_t & ctx) const = 0;
};

using statement_shptr_t = std::shared_ptr< statement_t >;

class logical_expression_t
	: public std::enable_shared_from_this< logical_expression_t >
{
public:
	virtual ~logical_expression_t() = default;

	[[nodiscard]]
	virtual bool
	exec(exec_context_t & ctx) const = 0;
};

using logical_expression_shptr_t = std::shared_ptr< logical_expression_t >;

namespace statements
{

class compound_stmt_t final : public statement_t
{
	const std::vector< statement_shptr_t > _statements;

public:
	compound_stmt_t(
		std::vector< statement_shptr_t > statements)
		: _statements{ std::move(statements) }
	{}

	void
	exec(exec_context_t & ctx) const override
	{
		for(const auto & stm : _statements)
			stm->exec(ctx);
	}
};

class while_loop_t final : public statement_t
{
	const logical_expression_shptr_t _condition;
	const statement_shptr_t _body;

public:
	while_loop_t(
		logical_expression_shptr_t condition,
		statement_shptr_t body)
		: _condition{ std::move(condition) }
		, _body{ std::move(body) }
	{}

	void
	exec(exec_context_t & ctx) const override
	{
		while( _condition->exec(ctx) )
		{
			_body->exec(ctx);
		}
	}
};

class assign_to_t final : public statement_t
{
	const std::string _var_name;
	const double _value;

public:
	assign_to_t(
		std::string var_name,
		double value)
		: _var_name{ std::move(var_name) }
		, _value{ value }
	{}

	void
	exec(exec_context_t & ctx) const override
	{
		ctx.assign_to(_var_name, _value);
	}
};

class increment_by_t final : public statement_t
{
	const std::string _var_name;
	const double _value_to_add;

public:
	increment_by_t(
		std::string var_name,
		double value_to_add)
		: _var_name{ std::move(var_name) }
		, _value_to_add{ value_to_add }
	{}

	void
	exec(exec_context_t & ctx) const override
	{
		ctx.get_for_modification(_var_name) += _value_to_add;
	}
};

class print_value_t final : public statement_t
{
	const std::string _var_name;

public:
	print_value_t(
		std::string var_name)
		: _var_name{ std::move(var_name) }
	{}

	void
	exec(exec_context_t & ctx) const override
	{
		const auto & v = ctx.get_for_modification(_var_name);
		std::cout << _var_name << "=" << v << std::endl;
	}
};

} /* namespace statements */

namespace expressions
{

class less_than_t final : public logical_expression_t
{
	const std::string _var_name;
	const double _value;

public:
	less_than_t(
		std::string var_name,
		double value)
		: _var_name{ std::move(var_name) }
		, _value{ value }
	{}

	bool
	exec(exec_context_t & ctx) const override
	{
		return ctx.get_for_modification(_var_name) < _value;
	}
};

} /* namespace expressions */

void
execute(const statement_shptr_t & what)
{
	try
	{
		exec_context_t ctx;
		what->exec(ctx);
	}
	catch(const std::exception & x)
	{
		std::cerr << "exception caught: " << x.what() << std::endl;
	}
}

} /* namespace script */

[[nodiscard]] script::statement_shptr_t
make_demo_script()
{
	static const std::string var_name{ "j" };

	std::vector< script::statement_shptr_t > statements;


	statements.push_back(
			std::make_shared< script::statements::assign_to_t >(
					var_name, 0));
	statements.push_back(
			std::make_shared< script::statements::while_loop_t >(
					std::make_shared< script::expressions::less_than_t >(
							var_name, 500'000'000),
					std::make_shared< script::statements::increment_by_t >(
							var_name, 1)
			)
	);
	statements.push_back(
			std::make_shared< script::statements::print_value_t >(
					var_name));

	return std::make_shared< script::statements::compound_stmt_t >(
			std::move(statements));
}

void
exec_demo_script_thread_body(
	const script::statement_shptr_t & stm,
	std::chrono::steady_clock::duration & time_receiver)
{
	raise_thread_priority();

	const auto started_at = std::chrono::steady_clock::now();
	script::execute(stm);
	const auto finished_at = std::chrono::steady_clock::now();

	time_receiver = finished_at - started_at;
}

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

	std::vector< std::jthread > threads;
	threads.reserve(threads_count);

	std::vector< std::chrono::steady_clock::duration > times{
			threads_count,
			std::chrono::steady_clock::duration::zero()
	};

	const auto demo_script = make_demo_script();

	for( std::size_t i = 0; i != threads_count; ++i )
	{
		threads.push_back(
			std::jthread{
				exec_demo_script_thread_body,
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

int main(int argc, char ** argv)
{
	try
	{
		do_work(argc, argv);
	}
	catch(const std::exception & x)
	{
		std::cout << "main: exception caught: " << x.what();
	}

	return 0;
}

