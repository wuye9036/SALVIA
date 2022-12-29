#pragma once

#include "salvia/common/colors.h"
#include <salvia/common/renderer_capacity.h>
#include <salvia/core/decl.h>
#include <salvia/core/viewport.h>
#include <salvia/resource/surface.h>

#include <eflib/math/math.h>
#include <eflib/memory/allocator.h>

#include <array>

namespace salvia::shader {

class vs_input {
public:
  vs_input() {}

  eflib::vec4 &attribute(size_t index) { return attributes_[index]; }

  eflib::vec4 const &attribute(size_t index) const { return attributes_[index]; }

private:
  typedef std::array<eflib::vec4, MAX_VS_INPUT_ATTRS> attribute_array;
  attribute_array attributes_;

  vs_input(const vs_input &rhs);
  vs_input &operator=(const vs_input &rhs);
};

class EFLIB_ALIGN(16) vs_output {
public:
  /*
  BUG FIX:
          Type A is aligned by A bytes, new A is A bytes aligned too, but address of new A[] is not
  aligned. It is known bug but not fixed yet. operator new/delete will ensure the address is
  aligned.
  */
  static void *operator new[](size_t size) {
    if (size == 0) {
      size = 1;
    }
    return eflib::aligned_malloc(size, 16);
  }

  static void operator delete[](void *p) {
    if (p == nullptr)
      return;
    return eflib::aligned_free(p);
  }

  enum attrib_modifier_type {
    am_linear = 1UL << 0,
    am_centroid = 1UL << 1,
    am_nointerpolation = 1UL << 2,
    am_noperspective = 1UL << 3,
    am_sample = 1UL << 4
  };

public:
  eflib::vec4 &position() { return registers_[0]; }

  eflib::vec4 const &position() const { return registers_[0]; }

  eflib::vec4 *attribute_data() { return registers_.data() + 1; }

  eflib::vec4 const *attribute_data() const { return registers_.data() + 1; }

  eflib::vec4 *raw_data() { return registers_.data(); }

  eflib::vec4 const *raw_data() const { return registers_.data(); }

  eflib::vec4 const &attribute(size_t index) const { return attribute_data()[index]; }

  eflib::vec4 &attribute(size_t index) { return attribute_data()[index]; }

  vs_output() {}

private:
  typedef std::array<eflib::vec4, MAX_VS_OUTPUT_ATTRS + 1> register_array;
  register_array registers_;

  vs_output(const vs_output &rhs);
  vs_output &operator=(const vs_output &rhs);
};

#if defined(EFLIB_MSVC)
#pragma warning(push)
#pragma warning(disable : 4324) // warning C4324: Structure was padded due to __declspec(align())
#endif

struct triangle_info {
  vs_output const *v0;
  bool front_face;
  EFLIB_ALIGN(16) eflib::vec4 bounding_box;
  EFLIB_ALIGN(16) eflib::vec4 edge_factors[3];
  vs_output ddx;
  vs_output ddy;

  triangle_info() {}
  triangle_info(triangle_info const & /*rhs*/) {}
  triangle_info &operator=(triangle_info const & /*rhs*/) { return *this; }
};

#if defined(EFLIB_MSVC)
#pragma warning(pop)
#endif

// vs_output compute_derivative
struct ps_output {
  std::array<eflib::vec4, MAX_RENDER_TARGETS> color;
};
} // namespace salvia::shader
