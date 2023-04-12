#pragma once

#include <salvia/core/render_stages.h>
#include <salvia/core/render_state.h>

#include <eflib/utility/shared_declaration.h>

namespace saliva::shader {
EFLIB_DECLARE_CLASS_SHARED_PTR(shader_object);
struct vs_input_op;
struct vs_output_op;
}  // namespace saliva::shader

namespace salvia::resource {
EFLIB_DECLARE_CLASS_SHARED_PTR(resource_manager);
}

namespace salvia::core {
struct renderer_parameters;
EFLIB_DECLARE_CLASS_SHARED_PTR(host);
EFLIB_DECLARE_CLASS_SHARED_PTR(render_core);
EFLIB_DECLARE_CLASS_SHARED_PTR(pixel_shader_unit);
EFLIB_DECLARE_CLASS_SHARED_PTR(stream_assembler);
EFLIB_DECLARE_STRUCT_SHARED_PTR(render_state);

class render_core {
private:
public:
  render_core();
  void update(render_state_ptr const& state);
  result execute();

private:
  uint64_t batch_id_;

  render_stages stages_;
  render_state_ptr state_;

  result draw();
  result clear_color();
  result clear_depth_stencil();
  void apply_shader_cbuffer();
  result async_start();
  result async_stop();
};

}  // namespace salvia::core