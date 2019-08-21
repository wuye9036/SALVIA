#ifndef SASL_CODEGEN_CG_GENERAL_H
#define SASL_CODEGEN_CG_GENERAL_H

#include <sasl/include/codegen/forward.h>

#include <sasl/include/codegen/cg_sisd.h>
#include <sasl/include/syntax_tree/visitor.h>
#include <sasl/include/semantic/reflector.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>

#include <memory>
#include <functional>
#include <any>
#include <unordered_map>
#include <string>

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
	class Constant;
}

enum class builtin_types: uint32_t;

BEGIN_NS_SASL_CODEGEN();

class module_vmcode_impl;
class module_vmcode;

class cg_general: public cg_sisd{
public:
	typedef cg_sisd parent_class;

	cg_general();

	SASL_VISIT_DCL( expression_list );

	SASL_VISIT_DCL( identifier );

	// declaration & type specifier
	SASL_VISIT_DCL( initializer );
	
	SASL_VISIT_DCL( member_initializer );
	SASL_VISIT_DCL( type_definition );
	SASL_VISIT_DCL( tynode );
	SASL_VISIT_DCL( alias_type );

	// statement
protected:
	SASL_SPECIFIC_VISIT_DCL( before_decls_visit	, program );
	SASL_SPECIFIC_VISIT_DCL( bin_logic			, binary_expression );
private:
	module_vmcode_impl* mod_ptr();
};

END_NS_SASL_CODEGEN();

#endif