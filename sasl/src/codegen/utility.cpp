#include <eflib/platform/config.h>

#include <sasl/codegen/utility.h>

#include <llvm/IR/Function.h>

using llvm::Function;

namespace sasl::codegen {

void dbg_print_blocks(Function *fn) {
#ifdef EFLIB_DEBUG
  /*printf( "Function: 0x%X\n", fn );
  for( Function::BasicBlockListType::iterator it = fn->getBasicBlockList().begin(); it !=
  fn->getBasicBlockList().end(); ++it ){ printf( "  Block: 0x%X\n", &(*it) );
  }*/
  fn = fn;
#else
  (void)fn;
#endif
}

} // namespace sasl::codegen
