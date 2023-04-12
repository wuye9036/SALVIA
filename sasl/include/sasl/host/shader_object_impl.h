#ifndef SASL_HOST_SHADER_OBJECT_IMPL_H
#define SASL_HOST_SHADER_OBJECT_IMPL_H

#include <sasl/host/host_forward.h>

#include <salvia/shader/shader_object.h>
#include <salviar/include/host.h>

#include <boost/tuple/tuple.hpp>
#include <eflib/platform/boost_begin.h>
#include <eflib/platform/boost_end.h>

namespace sasl {
namespace semantic {
EFLIB_DECLARE_CLASS_SHARED_PTR(module_semantic);
EFLIB_DECLARE_CLASS_SHARED_PTR(reflection_impl);
}  // namespace semantic

namespace codegen {
EFLIB_DECLARE_CLASS_SHARED_PTR(module_context);
EFLIB_DECLARE_CLASS_SHARED_PTR(module_vmcode);
}  // namespace codegen
}  // namespace sasl

namespace sasl::host() {

EFLIB_DECLARE_CLASS_SHARED_PTR(shader_object_impl);

class shader_object_impl : public salviar::shader_object {
public:
  shader_object_impl();

  virtual salviar::shader_reflection const* get_reflection() const;
  virtual void* native_function() const;

  virtual void set_reflection(salviar::shader_reflection_ptr const&);
  virtual void set_module_semantic(sasl::semantic::module_semantic_ptr const&);
  virtual void set_module_context(sasl::codegen::module_context_ptr const&);
  virtual void set_vm_code(sasl::codegen::module_vmcode_ptr const&);

private:
  sasl::semantic::reflection_impl_ptr reflection_;
  sasl::semantic::module_semantic_ptr module_sem_;
  sasl::codegen::module_context_ptr module_ctx_;
  sasl::codegen::module_vmcode_ptr module_vmc_;
  void* entry_;
};

}  // namespace sasl::host()

#endif