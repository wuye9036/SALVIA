#include <eflib/include/platform/disable_warnings.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>
#include <eflib/include/platform/enable_warnings.h>

#include <sasl/include/codegen/cg_api.h>

#include <vector>
#include <memory>

using llvm::Function;
using llvm::Module;
using llvm::raw_os_ostream;

using std::shared_ptr;

using std::vector;
using std::ostream;

BEGIN_NS_SASL_CODEGEN();

#if TODO
void optimize( shared_ptr<module_vmcode> code, vector<optimization_options> opt_options )
{
	Module* mod = code->get_vm_module();

	FunctionPassManager fpm(mod);

	for( optimization_options opt_option: opt_options ){
		switch ( opt_option ){
			case opt_verify:
				for( Function& f: mod->getFunctionList() ){
					if(!f.empty()){
						verifyFunction(f, PrintMessageAction);
					}
				}
				break;
			case opt_preset_std_for_function:
				// createStandardFunctionPasses( &fpm, 1 );
				break;
		}
	}

	fpm.doInitialization();

	for( Function& f: mod->getFunctionList() ){
		if(!f.empty()){
			fpm.run(f);
		}
	}
}
#endif

END_NS_SASL_CODEGEN();
