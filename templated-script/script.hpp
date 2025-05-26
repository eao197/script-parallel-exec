#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace script
{

template< typename T >
class exec_context_t
{
	std::unordered_map<std::string, T> _vars;

public:
	exec_context_t() = default;

	void
	assign_to(
		const std::string & name,
		T value)
	{
		_vars[name] = value;
	}

	T &
	get_mutable_ref(const std::string & name)
	{
		auto it = _vars.find(name);
		if( it == _vars.end() )
			throw std::runtime_error{ "there is no such variable: " + name };

		return it->second;
	}
};

template< typename T >
class statement_t : public std::enable_shared_from_this< statement_t<T> >
{
public:
	virtual ~statement_t() = default;

	virtual void
	exec(exec_context_t<T> & ctx) const = 0;
};

template< typename T >
using statement_shptr_t = std::shared_ptr< statement_t<T> >;

template< typename T >
class logical_expression_t
	: public std::enable_shared_from_this< logical_expression_t<T> >
{
public:
	virtual ~logical_expression_t() = default;

	[[nodiscard]]
	virtual bool
	exec(exec_context_t<T> & ctx) const = 0;
};

template< typename T >
using logical_expression_shptr_t = std::shared_ptr< logical_expression_t<T> >;

namespace statements
{

template< typename T >
class compound_stmt_t final : public statement_t<T>
{
	const std::vector< statement_shptr_t<T> > _statements;

public:
	compound_stmt_t(
		std::vector< statement_shptr_t<T> > statements)
		: _statements{ std::move(statements) }
	{}

	void
	exec(exec_context_t<T> & ctx) const override
	{
		for(const auto & stm : _statements)
			stm->exec(ctx);
	}
};

template< typename T >
class while_loop_t final : public statement_t<T>
{
	const logical_expression_shptr_t<T> _condition;
	const statement_shptr_t<T> _body;

public:
	while_loop_t(
		logical_expression_shptr_t<T> condition,
		statement_shptr_t<T> body)
		: _condition{ std::move(condition) }
		, _body{ std::move(body) }
	{}

	void
	exec(exec_context_t<T> & ctx) const override
	{
		while( _condition->exec(ctx) )
		{
			_body->exec(ctx);
		}
	}
};

template< typename T >
class assign_to_t final : public statement_t<T>
{
	const std::string _var_name;
	const T _value;

public:
	assign_to_t(
		std::string var_name,
		T value)
		: _var_name{ std::move(var_name) }
		, _value{ value }
	{}

	void
	exec(exec_context_t<T> & ctx) const override
	{
		ctx.assign_to(_var_name, _value);
	}
};

template< typename T >
class increment_by_t final : public statement_t<T>
{
	const std::string _var_name;
	const T _value_to_add;

public:
	increment_by_t(
		std::string var_name,
		T value_to_add)
		: _var_name{ std::move(var_name) }
		, _value_to_add{ value_to_add }
	{}

	void
	exec(exec_context_t<T> & ctx) const override
	{
		ctx.get_mutable_ref(_var_name) += _value_to_add;
	}
};

template< typename T >
class print_value_t final : public statement_t<T>
{
	const std::string _var_name;

public:
	print_value_t(
		std::string var_name)
		: _var_name{ std::move(var_name) }
	{}

	void
	exec(exec_context_t<T> & ctx) const override
	{
		const auto & v = ctx.get_mutable_ref(_var_name);
		std::osyncstream{ std::cout }
				<< _var_name << "=" << v << std::endl;
	}
};

} /* namespace statements */

namespace expressions
{

template< typename T >
class less_than_t final : public logical_expression_t<T>
{
	const std::string _var_name;
	const T _value;

public:
	less_than_t(
		std::string var_name,
		T value)
		: _var_name{ std::move(var_name) }
		, _value{ value }
	{}

	bool
	exec(exec_context_t<T> & ctx) const override
	{
		return ctx.get_mutable_ref(_var_name) < _value;
	}
};

} /* namespace expressions */

template< typename T >
void
execute(const statement_shptr_t<T> & what)
{
	try
	{
		exec_context_t<T> ctx;
		what->exec(ctx);
	}
	catch(const std::exception & x)
	{
		std::cerr << "exception caught: " << x.what() << std::endl;
	}
}

} /* namespace script */

