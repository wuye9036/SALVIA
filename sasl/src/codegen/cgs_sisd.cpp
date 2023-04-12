#include <sasl/codegen/cgs_sisd.h>

#include <sasl/codegen/cg_contexts.h>

#include <sasl/semantic/semantics.h>
#include <sasl/semantic/symbol.h>

#include <sasl/syntax_tree/declaration.h>

#include <sasl/enums/traits.h>

#include <eflib/diagnostics/assert.h>
#include <eflib/platform/cpuinfo.h>
#include <eflib/utility/unref_declarator.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace sasl::enums;

using llvm::APInt;
using llvm::BasicBlock;
using llvm::ConstantInt;
using llvm::DefaultIRBuilder;
using llvm::Function;
using llvm::PHINode;
using llvm::SwitchInst;
using llvm::Type;
using llvm::Value;

using std::string;
using std::vector;

namespace sasl::codegen {

namespace {
template <typename T>
APInt apint(T v) {
  return APInt(sizeof(v) << 3, static_cast<uint64_t>(v), std::is_signed<T>::value);
}

[[maybe_unused]] void mask_to_indexes(char indexes[4], uint32_t mask) {
  for (int i = 0; i < 4; ++i) {
    // XYZW is 1,2,3,4 but LLVM used 0,1,2,3
    char comp_index = static_cast<char>((mask >> i * 8) & 0xFF);
    if (comp_index == 0) {
      indexes[i] = -1;
      break;
    }
    indexes[i] = comp_index - 1;
  }
}

uint32_t indexes_to_mask(char indexes[4]) {
  uint32_t mask = 0;
  for (int i = 0; i < 4; ++i) {
    mask += (uint32_t)((indexes[i] + 1) << (i * 8));
  }
  return mask;
}

[[maybe_unused]] uint32_t indexes_to_mask(char idx0, char idx1, char idx2, char idx3) {
  char indexes[4] = {idx0, idx1, idx2, idx3};
  return indexes_to_mask(indexes);
}

[[maybe_unused]] void dbg_print_blocks(Function* fn) {
#if defined(EFLIB_DEBUG)
  /*printf( "Function: 0x%X\n", fn );
  for( Function::BasicBlockListType::iterator it = fn->getBasicBlockList().begin(); it !=
  fn->getBasicBlockList().end(); ++it ){ printf( "  Block: 0x%X\n", &(*it) );
  }*/
  fn = fn;
#else
  (void)fn;
#endif
}
}  // namespace

cgs_sisd::cgs_sisd() : cg_service(1) {
}

void cgs_sisd::store(multi_value& lhs, multi_value const& rhs) {
  assert(parallel_factor_ == 1);

  value_array src(parallel_factor_, nullptr);
  value_array address(parallel_factor_, nullptr);

  value_kinds kind = lhs.kind();

  if (kind == value_kinds::reference) {
    src = rhs.load(lhs.abi());
    address = lhs.raw();
  } else if (kind == value_kinds::elements) {
    elem_indexes indexes;
    multi_value const* root = nullptr;
    bool is_swizzle = merge_swizzle(root, indexes, lhs);

    if (is_swizzle && is_vector(root->hint())) {
      assert(lhs.parent()->storable());

      multi_value rhs_rvalue = rhs.to_rvalue();
      multi_value ret_v = root->to_rvalue();
      for (size_t i = 0; i < vector_size(rhs.hint()); ++i) {
        ret_v = emit_insert_val(ret_v, indexes[i], emit_extract_val(rhs_rvalue, i));
      }

      src = ret_v.load(lhs.abi());
      address = root->load_ref();
    } else {
      src = rhs.load(lhs.abi());
      address = lhs.load_ref();
    }
  }

  assert(valid_all(src));
  assert(valid_all(address));
  ext_->store(src, address);
}

multi_value cgs_sisd::cast_ints(multi_value const& v, cg_type* dest_tyi) {
  builtin_types hint_src = v.hint();
  builtin_types hint_dst = dest_tyi->hint();

  builtin_types scalar_hint_src = scalar_of(hint_src);

  Type* dest_ty = dest_tyi->ty(v.abi());
  Type* elem_ty = type_(scalar_of(hint_dst), abis::llvm);

  cast_ops::id op = is_signed(scalar_hint_src) ? cast_ops::i2i_signed : cast_ops::i2i_unsigned;
  unary_intrin_functor cast_sv_fn = ext_->bind_cast_sv(elem_ty, op);
  value_array val = ext_->call_unary_intrin(dest_ty, v.load(), cast_sv_fn);

  return create_value(dest_tyi, builtin_types::none, val, value_kinds::value, v.abi());
}

multi_value cgs_sisd::cast_i2f(multi_value const& v, cg_type* dest_tyi) {
  builtin_types hint_i = v.hint();
  builtin_types hint_f = dest_tyi->hint();

  builtin_types scalar_hint_i = scalar_of(hint_i);
  EFLIB_UNREF_DECLARATOR(scalar_hint_i);

  Type* dest_ty = dest_tyi->ty(v.abi());
  Type* elem_ty = type_(scalar_of(hint_f), abis::llvm);

  cast_ops::id op = is_signed(hint_i) ? cast_ops::i2f : cast_ops::u2f;
  unary_intrin_functor cast_sv_fn = ext_->bind_cast_sv(elem_ty, op);

  value_array val = ext_->call_unary_intrin(dest_ty, v.load(), cast_sv_fn);

  return create_value(dest_tyi, builtin_types::none, val, value_kinds::value, v.abi());
}

multi_value cgs_sisd::cast_f2i(multi_value const& v, cg_type* dest_tyi) {
  builtin_types hint_i = dest_tyi->hint();
  builtin_types hint_f = v.hint();
  EFLIB_UNREF_DECLARATOR(hint_f);

  builtin_types scalar_hint_i = scalar_of(hint_i);

  Type* dest_ty = dest_tyi->ty(v.abi());
  Type* elem_ty = type_(scalar_hint_i, abis::llvm);

  cast_ops::id op = is_signed(hint_i) ? cast_ops::f2i : cast_ops::f2u;
  unary_intrin_functor cast_sv_fn = ext_->bind_cast_sv(elem_ty, op);

  value_array val = ext_->call_unary_intrin(dest_ty, v.load(), cast_sv_fn);

  return create_value(dest_tyi, builtin_types::none, val, value_kinds::value, v.abi());
}

multi_value cgs_sisd::cast_f2f(multi_value const& v, cg_type* dest_tyi) {
  EFLIB_UNREF_DECLARATOR(v);
  EFLIB_UNREF_DECLARATOR(dest_tyi);

  ef_unimplemented();
  return multi_value();
}

multi_value cgs_sisd::create_vector(std::vector<multi_value> const& scalars, abis abi) {
  builtin_types scalar_hint = scalars[0].hint();
  builtin_types hint = vector_of(scalar_hint, scalars.size());

  multi_value ret = undef_value(hint, abi);
  for (size_t i = 0; i < scalars.size(); ++i) {
    ret = emit_insert_val(ret, (int)i, scalars[i]);
  }
  return ret;
}

void cgs_sisd::emit_return() {
  builder().CreateRetVoid();
}

void cgs_sisd::emit_return(multi_value const& ret_v, abis abi) {
  if (abi == abis::unknown) {
    abi = fn().abi();
  }

  if (fn().return_via_arg()) {
    ext_->store(ret_v.load(abi), fn().return_address());
    builder().CreateRetVoid();
  } else {
    Value* ret_value = nullptr;
    if (fn().multi_value_args()) {
      ret_value = ext_->get_array(ret_v.load(abi));
    } else {
      ret_value = ret_v.load(abi)[0];
    }
    builder().CreateRet(ret_value);
  }
}

Value* cgs_sisd::select_(Value* cond, Value* yes, Value* no) {
  return builder().CreateSelect(cond, yes, no);
}

bool cgs_sisd::prefer_externals() const {
  return false;
}

bool cgs_sisd::prefer_scalar_code() const {
  return false;
}

multi_value cgs_sisd::emit_swizzle(multi_value const& lhs, uint32_t mask) {
  EFLIB_UNREF_DECLARATOR(lhs);
  EFLIB_UNREF_DECLARATOR(mask);

  ef_unimplemented();
  return multi_value();
}

multi_value cgs_sisd::emit_write_mask(multi_value const& vec, uint32_t mask) {
  ef_unimplemented();
  EFLIB_UNREF_DECLARATOR(vec);
  EFLIB_UNREF_DECLARATOR(mask);

  return multi_value();
}

void cgs_sisd::switch_to(multi_value const& cond,
                         std::vector<std::pair<multi_value, insert_point_t>> const& cases,
                         insert_point_t const& default_branch) {
  assert(parallel_factor_ == 1);
  Value* v = cond.load()[0];
  SwitchInst* inst =
      builder().CreateSwitch(v, default_branch.block, static_cast<unsigned>(cases.size()));
  for (size_t i_case = 0; i_case < cases.size(); ++i_case) {
    inst->addCase(llvm::cast<ConstantInt>(cases[i_case].first.load()[0]),
                  cases[i_case].second.block);
  }
}

multi_value cgs_sisd::cast_i2b(multi_value const& v) {
  assert(is_integer(v.hint()));
  return emit_cmp_ne(v, null_value(v.hint(), v.abi()));
}

multi_value cgs_sisd::cast_f2b(multi_value const& v) {
  assert(is_real(v.hint()));
  return emit_cmp_ne(v, null_value(v.hint(), v.abi()));
}

multi_value cgs_sisd::emit_ddx(multi_value const& v) {
  // It is not available in SISD mode.
  ef_unimplemented();
  return v;
}

multi_value cgs_sisd::emit_ddy(multi_value const& v) {
  // It is not available in SISD mode.
  ef_unimplemented();
  return v;
}

Value* cgs_sisd::phi_(BasicBlock* b0, Value* v0, BasicBlock* b1, Value* v1) {
  PHINode* phi = builder().CreatePHI(v0->getType(), 2);
  phi->addIncoming(v0, b0);
  phi->addIncoming(v1, b1);
  return phi;
}

}  // namespace sasl::codegen
