#pragma once

#include <eflib/math/vector.h>

#include <salvia/common/renderer_capacity.h>

#include <array>

namespace salvia::core {
struct viewport;
}
namespace salvia::shader {

class vs_output;

struct vs_input_op {
  typedef vs_input& (*vs_input_construct)(vs_input& out, eflib::vec4 const* attrs);
  typedef vs_input& (*vs_input_copy)(vs_input& out, const vs_input& in);

  vs_input_construct construct;
  vs_input_copy copy;
};

namespace vs_output_functions {
using eflib::vec4;

typedef vs_output& (*construct)(vs_output& out, vec4 const& position, vec4 const* attrs);
typedef vs_output& (*copy)(vs_output& out, const vs_output& in);

typedef vs_output& (*project)(vs_output& out, const vs_output& in);
typedef vs_output& (*unproject)(vs_output& out, const vs_output& in);

typedef vs_output& (*add)(vs_output& out, const vs_output& vso0, const vs_output& vso1);
typedef vs_output& (*sub)(vs_output& out, const vs_output& vso0, const vs_output& vso1);
typedef vs_output& (*mul)(vs_output& out, const vs_output& vso0, float f);
typedef vs_output& (*div)(vs_output& out, const vs_output& vso0, float f);

typedef void (*compute_derivative)(
    vs_output& ddx, vs_output& ddy, vs_output const& e01, vs_output const& e02, float inv_area);

typedef vs_output& (*lerp)(vs_output& out,
                           const vs_output& start,
                           const vs_output& end,
                           float step);
typedef vs_output& (*step_2d_unproj)(vs_output& out,
                                     vs_output const& start,
                                     float step0,
                                     vs_output const& derivation0,
                                     float step1,
                                     vs_output const& derivation1);
typedef vs_output& (*step_2d_unproj_quad)(vs_output* out,
                                          vs_output const& start,
                                          float step0,
                                          vs_output const& derivation0,
                                          float step1,
                                          vs_output const& derivation1);
}  // namespace vs_output_functions

struct vs_output_op {
  vs_output_functions::construct construct;
  vs_output_functions::copy copy;

  vs_output_functions::project project;
  vs_output_functions::unproject unproject;

  vs_output_functions::add add;
  vs_output_functions::sub sub;
  vs_output_functions::mul mul;
  vs_output_functions::div div;

  vs_output_functions::lerp lerp;
  vs_output_functions::step_2d_unproj step_2d_unproj_pos;
  vs_output_functions::step_2d_unproj step_2d_unproj_attr;
  vs_output_functions::step_2d_unproj_quad step_2d_unproj_pos_quad;
  vs_output_functions::step_2d_unproj_quad step_2d_unproj_attr_quad;

  vs_output_functions::compute_derivative compute_derivative;

  typedef std::array<uint32_t, MAX_VS_OUTPUT_ATTRS> interpolation_modifier_array;
  interpolation_modifier_array attribute_modifiers;
};

}  // namespace salvia::shader
