#pragma once

#include <iostream>
#include <optional>
#include <variant>
#include <vector>

namespace run_params
{

/// Тип для представления номера группы процессоров.
using processor_group_id_t = unsigned short;

/// Тип для представления номера логического процессора в группе.
using processor_number_t = unsigned short;

/// Информация о привязке одного треда.
struct thread_pinning_info_t
{
	/// Группа, к которой должна быть привязана нить.
	processor_group_id_t _group;

	/// Процессор внутри группы, к которому должна быть привязана нить.
	processor_number_t _processor;

	thread_pinning_info_t(
		processor_group_id_t group,
		processor_number_t processor )
		: _group{ group }
		, _processor{ processor }
	{}
};

inline std::ostream &
operator<<( std::ostream & to, const thread_pinning_info_t & what )
{
	return to << what._group << "-"
			<< static_cast<unsigned short>(what._processor);
}

/// Для случая, когда привязываться вообще не нужно.
struct no_pinning_t
{};

/// Для случая, когда нужно привязывать к имеющимся ядрам
/// последовательно.
struct seq_pinning_t
{};

/// Для случая, когда нужно привязывать к конкретным ядрам.
struct selective_pinning_t
{
	std::vector< thread_pinning_info_t > _cores;
};

/// Информация о том, нужно ли привязывать рабочие нити к конкретным
/// ядрам или нет.
using pinning_params_t = std::variant<
		no_pinning_t,
		seq_pinning_t,
		selective_pinning_t
	>;

/// Информация о том, сколько нитей нужно создать и к каким ядрам их
/// нужно привязывать (если вообще нужно).
struct run_params_t
{
	/// Сколько рабочих нитей нужно создать.
	///
	/// Может отсутствовать если задан selective_pinning_t с
	/// перечнем конкретных ядер.
	std::optional< unsigned > _threads_count{};

	/// Нужно ли привязывать нити к ядрам и если нужно то как именно.
	pinning_params_t _pinning{ no_pinning_t{} };
};

/// Индикатор того, что была запрошена помощь по параметрам командной строки.
struct help_requested_t
{};

/// Тип для результата разбора командной строки.
using args_parsing_result_t = std::variant<
		help_requested_t,
		run_params_t
	>;

/// Разобрать коммандную строку и получить параметры для работы.
[[nodiscard]]
args_parsing_result_t
parse_cmd_line_args( int argc, char ** argv );

} /* namespace run_params */

