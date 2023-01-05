#pragma once

#include <eflib/platform/cpuinfo.h>
#include <salvia/shader/constants.h>

#include <vector>

namespace salvia::shader {

struct shader_profile {
  salvia::shader::languages language;
};

#define SALVIA_LVT_VECTOR_OF(scalar, length)
#define SALVIA_LVT_MATRIX_OF(scalar, vector_size, vector_count)

namespace details {
uint32_t const dimension_field_offset = 28;
uint32_t const classification_field_offset = 24;
uint32_t const sign_field_offset = 20;
uint32_t const precision_field_offset = 16;
uint32_t const scalar_field_offset = 16;
uint32_t const vector_size_field_offset = 8;
uint32_t const vector_count_field_offset = 0;

uint32_t const dimension_mask = 0xFU << dimension_field_offset;
uint32_t const scalar_flag = 0U << dimension_field_offset;
uint32_t const vector_flag = 1U << dimension_field_offset;
uint32_t const matrix_flag = 2U << dimension_field_offset;

uint32_t const scalar_mask = (1U << dimension_field_offset) - (1U << scalar_field_offset);
uint32_t const vector_size_mask = 0xFF00U << vector_size_field_offset;
uint32_t const vector_count_mask = 0xFFU;

uint32_t const real_class = 1U << classification_field_offset;
uint32_t const integer_class = 2U << classification_field_offset;
uint32_t const boolean_class = 3U << classification_field_offset;
uint32_t const void_class = 4U << classification_field_offset;
uint32_t const sampler_class = 5U << classification_field_offset;
uint32_t const classification_mask = 0xFU << classification_field_offset;

uint32_t const signed_flag = (1U << sign_field_offset) + integer_class;
uint32_t const unsigned_flag = (2U << sign_field_offset) + integer_class;
uint32_t const sign_mask = 0xFFU << sign_field_offset;
} // namespace details

enum language_value_types {
  lvt_none = 0,

  lvt_void = details::void_class,
  lvt_boolean = details::boolean_class,

  lvt_sint8 = (1U << details::scalar_field_offset) + details::signed_flag,
  lvt_sint16 = (2U << details::scalar_field_offset) + details::signed_flag,
  lvt_sint32 = (3U << details::scalar_field_offset) + details::signed_flag,
  lvt_sint64 = (4U << details::scalar_field_offset) + details::signed_flag,

  lvt_uint8 = (1U << details::scalar_field_offset) + details::unsigned_flag,
  lvt_uint16 = (2U << details::scalar_field_offset) + details::unsigned_flag,
  lvt_uint32 = (3U << details::scalar_field_offset) + details::unsigned_flag,
  lvt_uint64 = (4U << details::scalar_field_offset) + details::unsigned_flag,

  lvt_float = (1U << details::scalar_field_offset) + details::real_class,
  lvt_double = (2U << details::scalar_field_offset) + details::real_class,

  lvt_f32v1 = lvt_float | details::vector_flag | (1 << details::vector_size_field_offset),
  lvt_f32v2 = lvt_float | details::vector_flag | (2 << details::vector_size_field_offset),
  lvt_f32v3 = lvt_float | details::vector_flag | (3 << details::vector_size_field_offset),
  lvt_f32v4 = lvt_float | details::vector_flag | (4 << details::vector_size_field_offset),

  lvt_f32m44 = lvt_float | details::matrix_flag | (4 << details::vector_size_field_offset) |
               (4 << details::vector_count_field_offset)
};

enum aggregation_types { aggt_none, aggt_array };

enum sv_usage {
  su_none = 0,

  su_stream_in,
  su_buffer_in,
  su_stream_out,
  su_buffer_out,

  sv_usage_count
};

enum class reg_categories : uint32_t {
  unknown = 0,
  offset,   // A special category, which register will be reallocated to correct category.
  uniforms, // cb#, s#, t#
  varying,  // v#
  outputs,  // o#
  count
};

static uint32_t const REG_CATEGORY_REGFILE_COUNTS[] = {
    0,               // Unknown
    0,               // Offset
    16 + 16 + 2 + 2, // Uniform: CB(16) + ICB(16) + Samplers + Textures + Global + Params
    1,               // Varying
    1                // Output
};

struct sv_layout {
  sv_layout()
      : logical_index(0), physical_index(0), offset(0), size(0), padding(0), usage(su_none),
        value_type(lvt_none), agg_type(aggt_none), sv(sv_none) {}

  size_t total_size() const { return size + padding; }

  size_t logical_index;
  size_t physical_index;

  size_t offset;
  size_t size;
  size_t padding;

  sv_usage usage;

  language_value_types value_type;
  aggregation_types agg_type;
  int internal_type; // Type used in SASL.

  semantic_value sv;
};

inline constexpr int PACKAGE_ELEMENT_COUNT = 4;
inline constexpr int PACKAGE_LINE_ELEMENT_COUNT = 2;
inline constexpr int SIMD_ELEMENT_COUNT = 4;

inline constexpr size_t REGISTER_SIZE = 16;

// ! Reflection of shader.
//
class shader_reflection {
public:
  virtual languages get_language() const = 0;
  virtual std::string_view entry_name() const = 0;
  virtual std::vector<sv_layout *> layouts(sv_usage usage) const = 0;
  virtual size_t layouts_count(sv_usage usage) const = 0;
  virtual size_t total_size(sv_usage usage) const = 0;
  virtual sv_layout *input_sv_layout(salvia::shader::semantic_value const &) const = 0;
  virtual sv_layout *input_sv_layout(std::string_view) const = 0;
  virtual sv_layout *output_sv_layout(salvia::shader::semantic_value const &) const = 0;
  virtual bool has_position_output() const = 0;

  virtual ~shader_reflection() {}
};

struct rfile_name {
  rfile_name() : cat(reg_categories::unknown), index(0) {}

  rfile_name(reg_categories cat, uint32_t index) : cat(cat), index(index) {}

  rfile_name(rfile_name const &rhs) : cat(rhs.cat), index(rhs.index) {}

  rfile_name &operator=(rfile_name const &rhs) {
    cat = rhs.cat;
    index = rhs.index;
    return *this;
  }

  bool operator==(rfile_name const &rhs) const { return cat == rhs.cat && index == rhs.index; }

  static rfile_name global() { return rfile_name(reg_categories::uniforms, 34 + 0); }

  static rfile_name params() { return rfile_name(reg_categories::uniforms, 34 + 1); }

  static rfile_name varyings() { return rfile_name(reg_categories::varying, 0); }

  static rfile_name outputs() { return rfile_name(reg_categories::outputs, 0); }

  reg_categories cat;
  uint32_t index;
};

struct reg_name {
  rfile_name rfile;
  uint32_t reg_index;
  uint32_t elem;

  reg_name() : reg_index(0), elem(0) {}

  reg_name(reg_categories cat, uint32_t rfile_index, uint32_t reg_index, uint32_t elem)
      : rfile(cat, rfile_index), reg_index(reg_index), elem(elem) {}

  reg_name(rfile_name rfile, uint32_t reg_index, uint32_t elem)
      : rfile(rfile), reg_index(reg_index), elem(elem) {}

  reg_name(reg_name const &rhs) : rfile(rhs.rfile), reg_index(rhs.reg_index), elem(rhs.elem) {}

  reg_name &operator=(reg_name const &rhs) {
    rfile = rhs.rfile;
    reg_index = rhs.reg_index;
    elem = rhs.elem;
    return *this;
  }

  reg_name advance(size_t byte_offsets) const {
    assert(byte_offsets >= 0);
    size_t adv_elem_count = (byte_offsets / 4) % 4;
    size_t adv_reg_count = byte_offsets / REGISTER_SIZE;

    return reg_name(rfile,
                    static_cast<uint32_t>(reg_index + adv_reg_count + (adv_elem_count + elem) / 4),
                    static_cast<uint32_t>((adv_elem_count + elem) % 4));
  }

  reg_name align_up_to_reg() const {
    return reg_name(rfile, elem > 0 ? reg_index + 1 : reg_index, 0);
  }

  bool valid() const { return rfile.cat != reg_categories::unknown; }

  bool operator<(reg_name const &rhs) const {
    assert(rfile == rhs.rfile);
    if (reg_index < rhs.reg_index)
      return true;
    if (reg_index > rhs.reg_index)
      return false;
    return elem < rhs.elem;
  }
};

// Shader reflection for register-based shader.
class shader_reflection2 {
public:
  virtual languages language() const = 0;
  virtual std::string_view entry_name() const = 0;
  virtual std::vector<semantic_value> varying_semantics() const = 0;
  virtual size_t available_reg_count(reg_categories cat) const = 0;
  virtual reg_name find_reg(reg_categories cat, semantic_value const &sv) const = 0;
  virtual size_t reg_addr(reg_name const &rname) const = 0;
  virtual ~shader_reflection2() {}
};
} // namespace salvia::shader
