#ifndef SASL_CODE_GENERATOR_JIT_API_H
#define SASL_CODE_GENERATOR_JIT_API_H

#include <sasl/include/codegen/forward.h>

#include <eflib/string/ustring.h>

#include <eflib/platform/disable_warnings.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <eflib/platform/enable_warnings.h>

#include <vector>

namespace sasl::codegen {

class codegen_context;

class jit_engine{
public:
	virtual void* get_function(eflib::fixed_string const& func_name ) = 0;
	virtual void inject_function( void* fn, eflib::fixed_string const& fn_name ) = 0;
protected:
	jit_engine(){}
	virtual ~jit_engine(){}
private:
	jit_engine( const jit_engine& );
	jit_engine& operator = (const jit_engine& );
};

}

#endif