#ifndef SASL_CODEGEN_CG_CONTEXTS_H
#define SASL_CODEGEN_CG_CONTEXTS_H

#include <sasl/include/codegen/forward.h>

#include <sasl/include/codegen/cgs_sisd.h>

#include <eflib/include/platform/typedefs.h>
#include <eflib/include/utility/shared_declaration.h>

#include <eflib/include/diagnostics/assert.h>

#include <memory>

namespace llvm{
	class AllocaInst;
	class Argument;
	class Constant;
	class Function;
	class GlobalVariable;
	class Value;
	class BasicBlock;
	
	class Type;
	class StructType;

	class ReturnInst;

	class LLVMContext;
	class Module;

    class IRBuilderDefaultInserter;
    template <typename T, typename Inserter> class IRBuilder;
    class ConstantFolder;
    using DefaultIRBuilder = IRBuilder<ConstantFolder, IRBuilderDefaultInserter>;
}

namespace sasl
{
	namespace semantic
	{
		class symbol;
	}
}

BEGIN_NS_SASL_CODEGEN();

EFLIB_DECLARE_CLASS_SHARED_PTR(module_context);

class module_context
{
public:
	static std::shared_ptr<module_context> create();

	virtual node_context*	get_node_context(sasl::syntax_tree::node const*) const = 0;
	virtual node_context*	get_or_create_node_context(sasl::syntax_tree::node const*) = 0;
	virtual node_context*	create_temporary_node_context() = 0;

	virtual cg_type*		create_cg_type() = 0;
	virtual cg_function*	create_cg_function() = 0;

	virtual ~module_context(){}
};

struct node_context
{
	node_context(module_context* owner)
		: owner(owner)
		, function_scope(NULL)
		, is_semantic_mode(false)
		, ty(NULL)
		, declarator_count(0)
		, node_value(0)
	{
	}

	module_context*	owner;
	cg_function*	function_scope;		///< Function type.
	multi_value		node_value;			///< Value attached to node.
	bool			is_semantic_mode;	///< Expression is a semantic mode. In this mode, the memory get from semantic but not
	cg_type*		ty;					///< Type attached to node.
	insert_point_t	label_position;		///< For labeled statement
	int				declarator_count;	///< The declarator count of declaration.
};

END_NS_SASL_CODEGEN();

#endif