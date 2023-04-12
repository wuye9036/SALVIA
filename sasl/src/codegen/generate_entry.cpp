#include <sasl/codegen/generate_entry.h>

#include <sasl/codegen/cgs.h>
#include <sasl/enums/traits.h>
#include <sasl/semantic/reflection_impl.h>

#include <salvia/shader/reflection.h>

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#include <vector>

using llvm::ArrayType;
using llvm::DataLayout;
using llvm::PointerType;
using llvm::StructLayout;
using llvm::StructType;
using llvm::Type;
using salvia::shader::su_buffer_in;
using salvia::shader::su_buffer_out;
using salvia::shader::su_stream_in;
using salvia::shader::su_stream_out;
using salvia::shader::sv_layout;
using salvia::shader::sv_usage;
using salvia::shader::sv_usage_count;
using sasl::semantic::reflection_impl;
using std::make_pair;
using std::pair;
using std::vector;

using namespace sasl::enums;

namespace sasl::codegen {

bool compare_layout(pair<sv_layout*, Type*> const& lhs, pair<sv_layout*, Type*> const& rhs) {
  return lhs.first->size < rhs.first->size;
}

/// Re-arrange layouts will Sort up struct members by storage size.
/// For e.g.
///   struct { int i; byte b; float f; ushort us; }
/// Will be arranged to
///   struct { byte b; ushort us; int i; float f; };
/// It will minimize the structure size.
void sort_struct_members(vector<sv_layout*>& sorted_layouts,
                         vector<Type*>& sorted_tys,
                         vector<sv_layout*> const& layouts,
                         vector<Type*> const& tys,
                         DataLayout const* dataLayout) {
  size_t layouts_count = layouts.size();
  vector<size_t> elems_size;
  elems_size.reserve(layouts_count);

  vector<pair<sv_layout*, Type*>> layout_ty_pairs;
  layout_ty_pairs.reserve(layouts_count);

  auto layout_it = layouts.begin();
  for (Type* ty : tys) {
    size_t sz = (size_t)dataLayout->getTypeStoreSize(ty);
    (*layout_it)->size = sz;
    layout_ty_pairs.emplace_back(*layout_it, ty);
    ++layout_it;
  }

  std::sort(layout_ty_pairs.begin(), layout_ty_pairs.end(), compare_layout);

  sorted_layouts.clear();
  sorted_tys.clear();

  for (auto const& layout_ty_pair : layout_ty_pairs) {
    sorted_layouts.push_back(layout_ty_pair.first);
    sorted_tys.push_back(layout_ty_pair.second);
  }
}

Type* generate_parameter_type(reflection_impl const* abii, sv_usage su, cg_service* cg) {
  vector<sv_layout*> svls = abii->layouts(su);
  vector<Type*> sem_tys;
  vector<builtin_types> tycodes;

  auto const& dataLayout = cg->module()->getDataLayout();

  for (sv_layout* svl : svls) {
    builtin_types value_bt = sasl::enums::to_builtin_types(svl->value_type);
    tycodes.push_back(value_bt);
    Type* value_vm_ty = cg->type_(value_bt, abis::c);

    // The stream of vertex shader is composed by pointer, like this
    // struct stream { float4* position; float3* normal; };
    // Otherwise the members of stream are values at all.
    if ((su_stream_in == su || su_stream_out == su ||
         svl->agg_type == salvia::shader::aggt_array) &&
        cg->parallel_factor() == 1) {
      sem_tys.push_back(PointerType::getUnqual(value_vm_ty));
    } else {
      sem_tys.push_back(value_vm_ty);
    }
    svl->size = static_cast<size_t>(dataLayout.getTypeStoreSize(value_vm_ty));
  }

  if (su_buffer_in == su || su_buffer_out == su) {
    sort_struct_members(svls, sem_tys, svls, sem_tys, &dataLayout);
  }

  char const* param_struct_name = nullptr;
  switch (su) {
  case su_stream_in: param_struct_name = ".s.stri"; break;
  case su_buffer_in: param_struct_name = ".s.bufi"; break;
  case su_stream_out: param_struct_name = ".s.stro"; break;
  case su_buffer_out: param_struct_name = ".s.bufo"; break;
  default: ef_unreachable("Unrecognized SV usage."); break;
  }
  assert(param_struct_name);

  // If tys is empty, placeholder (int8) will be inserted
  StructType* param_struct = nullptr;
  if (sem_tys.empty()) {
    param_struct = StructType::create(param_struct_name, Type::getInt8Ty(cg->context()));
  } else {
    param_struct = StructType::create(sem_tys, param_struct_name);
  }

  // Update Layout physical information.
  StructLayout const* struct_layout = dataLayout.getStructLayout(param_struct);

  size_t next_offset = 0;
  for (size_t i_elem = 0; i_elem < svls.size(); ++i_elem) {
    size_t offset = next_offset;

    size_t next_i_elem = i_elem + 1;
    if (next_i_elem < sem_tys.size()) {
      next_offset = (size_t)struct_layout->getElementOffset(static_cast<unsigned>(next_i_elem));
    } else {
      next_offset = (size_t)struct_layout->getSizeInBytes();
      const_cast<reflection_impl*>(abii)->update_size(next_offset, su);
    }

    svls[i_elem]->offset = offset;
    svls[i_elem]->physical_index = i_elem;
    svls[i_elem]->padding = (next_offset - offset) - svls[i_elem]->size;
  }

  return param_struct;
}

vector<Type*> generate_vs_entry_param_type(reflection_impl const* abii, cg_service* cg) {
  assert(cg->parallel_factor() == 1);
  vector<Type*> ret(sv_usage_count, nullptr);

  ret[su_buffer_in] = generate_parameter_type(abii, su_buffer_in, cg);
  ret[su_buffer_out] = generate_parameter_type(abii, su_buffer_out, cg);
  ret[su_stream_in] = generate_parameter_type(abii, su_stream_in, cg);
  ret[su_stream_out] = generate_parameter_type(abii, su_stream_out, cg);

  for (size_t i = 1; i < sv_usage_count; ++i) {
    ret[i] = PointerType::getUnqual(ret[i]);
  }

  return {ret.begin() + 1, ret.end()};
}

vector<Type*> generate_ps_entry_param_type(reflection_impl const* abii, cg_service* cg) {
  assert(cg->parallel_factor() > 1);
  vector<Type*> ret(sv_usage_count, nullptr);

  ret[su_buffer_in] = generate_parameter_type(abii, su_buffer_in, cg);
  ret[su_buffer_out] = generate_parameter_type(abii, su_buffer_out, cg);
  ret[su_stream_in] = generate_parameter_type(abii, su_stream_in, cg);
  ret[su_stream_out] = generate_parameter_type(abii, su_stream_out, cg);

  for (size_t i = 1; i < sv_usage_count; ++i) {
    auto usage = static_cast<sv_usage>(i);
    // Only one copy of data in buffer even if it is wrapped mode.
    if (usage == su_buffer_in || usage == su_buffer_out) {
      ret[i] = PointerType::getUnqual(ret[i]);
    } else {
      ret[i] = PointerType::getUnqual(ret[i]);
      ret[i] = ArrayType::get(ret[i], cg->parallel_factor());
      ret[i] = PointerType::getUnqual(ret[i]);
    }
  }

  return {ret.begin() + 1, ret.end()};
}

}  // namespace sasl::codegen