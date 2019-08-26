#ifndef SASL_CODEGEN_CG_VS_H
#define SASL_CODEGEN_CG_VS_H

#include <sasl/include/codegen/cg_sisd.h>

#include <sasl/include/semantic/reflection_impl.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/utility/value_init.hpp>
#include <eflib/include/platform/boost_end.h>

#include <memory>

namespace sasl{
	namespace semantic{
		class caster_t;
		class module_semantic;
	}
	namespace syntax_tree{
		struct expression;
		struct tynode;
		struct node;
	}
}

namespace llvm{
	class PointerType;
	class StructType;
	class DataLayout;
}

BEGIN_NS_SASL_CODEGEN();

class cg_vs: public cg_sisd
{
public:
	typedef cg_sisd parent_class;

	cg_vs();
	~cg_vs();

	// expressions
	SASL_VISIT_DCL( member_expression );
	SASL_VISIT_DCL( cast_expression );
	SASL_VISIT_DCL( expression_list );
	SASL_VISIT_DCL( cond_expression );
	
	SASL_VISIT_DCL( variable_expression );
	SASL_VISIT_DCL( identifier );

	// declaration & type specifier
	SASL_VISIT_DCL( initializer );
	SASL_VISIT_DCL( member_initializer );
	SASL_VISIT_DCL( declaration );
	SASL_VISIT_DCL( type_definition );
	SASL_VISIT_DCL( tynode );
	SASL_VISIT_DCL( alias_type );

private:
	SASL_SPECIFIC_VISIT_DCL( before_decls_visit, program );

	// Binary logical operators.
	SASL_SPECIFIC_VISIT_DCL( bin_logic, binary_expression );

	// Overrides them for generating entry function if need.
	SASL_SPECIFIC_VISIT_DCL( create_fnsig,			function_def );
	SASL_SPECIFIC_VISIT_DCL( create_fnargs,			function_def );
	SASL_SPECIFIC_VISIT_DCL( create_virtual_args,	function_def );

	SASL_SPECIFIC_VISIT_DCL( visit_return, jump_statement );

	bool is_entry( llvm::Function* ) const;

	module_vmcode_impl* mod_ptr();

	multi_value layout_to_value(salviar::sv_layout* si, bool copy_from_input);

	// If ctxt is NULL, the generated value and type will be cached.
	// Return true if context is fetched from cache.
	bool layout_to_node_context(
		node_context* ctxt, salviar::sv_layout* si,
		bool store_to_existed_value, bool copy_from_input);

	void copy_to_result( std::shared_ptr<sasl::syntax_tree::expression> const& );
	void copy_to_agg_result( node_context* data );

	llvm::Function* entry_fn;
	sasl::semantic::symbol* entry_sym;

	multi_value param_values[salviar::sv_usage_count];

	typedef std::unordered_map<salviar::semantic_value, node_context*> input_copies_dict;
	input_copies_dict input_copies_;
};

END_NS_SASL_CODEGEN();

#endif