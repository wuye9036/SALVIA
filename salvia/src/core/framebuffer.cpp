#include <salvia/core/framebuffer.h>

#include <salvia/core/render_state.h>
#include <salvia/core/renderer.h>
#include <salvia/resource/pixel_accessor.h>
#include <salvia/resource/surface.h>
#include <salvia/shader/shader_regs.h>
#include <salvia/shader/shader_regs_op.h>

#include <eflib/math/collision_detection.h>

#include <algorithm>

using namespace eflib;
using namespace std;
using namespace salvia::shader;
using namespace salvia::resource;

namespace salvia::core {

template <uint32_t Format>
class depth_stencil_accessor {
public:
  static float read_depth(void const* /*ds_data*/) {}

  static uint32_t read_stencil(void const* /*ds_data*/) {}

  static void read_depth_stencil(float& /*depth*/, uint32_t& /*stencil*/, void const* /*ds_data*/) {
  }

  static void write_depth(void* /*ds_data*/, float /*depth*/) {}

  static void write_stencil(void* /*ds_data*/, uint32_t /*stencil*/) {}

  static void write_depth_stencil(void* /*ds_data*/, float /*depth*/, uint32_t /*stencil*/) {}
};

template <>
class depth_stencil_accessor<pixel_format_color_rg32f> {
public:
  static float read_depth(void const* ds_data) {
    return reinterpret_cast<color_rg32f const*>(ds_data)->r;
  }

  static uint32_t read_stencil(void const* ds_data) {
    union {
      float stencil_f;
      uint32_t stencil_u;
    };
    stencil_f = reinterpret_cast<color_rg32f const*>(ds_data)->g;
    return stencil_u;
  }

  static void read_depth_stencil(float& depth, uint32_t& stencil, void const* ds_data) {
    depth = reinterpret_cast<color_rg32f const*>(ds_data)->r;
    union {
      float stencil_f;
      uint32_t stencil_u;
    };
    stencil_f = reinterpret_cast<color_rg32f const*>(ds_data)->g;
    stencil = stencil_u;
  }

  static void write_depth(void* ds_data, float depth) {
    reinterpret_cast<color_rg32f*>(ds_data)->r = depth;
  }

  static void write_stencil(void* ds_data, uint32_t stencil) {
    union {
      float stencil_f;
      uint32_t stencil_u;
    };
    stencil_u = stencil;
    reinterpret_cast<color_rg32f*>(ds_data)->g = stencil_f;
  }

  static void write_depth_stencil(void* ds_data, float depth, uint32_t stencil) {
    reinterpret_cast<color_rg32f*>(ds_data)->r = depth;
    union {
      float stencil_f;
      uint32_t stencil_u;
    };
    stencil_u = stencil;
    reinterpret_cast<color_rg32f*>(ds_data)->g = stencil_f;
  }
};

uint32_t mask_stencil_0(uint32_t /*stencil*/, uint32_t /*mask*/) {
  return 0;
}

uint32_t mask_stencil_1(uint32_t stencil, uint32_t mask) {
  return stencil & mask;
}

template <typename T>
bool compare_never(T /*lhs*/, T /*rhs*/) {
  return false;
}

template <typename T>
bool compare_less(T lhs, T rhs) {
  return lhs < rhs;
}

template <typename T>
bool compare_equal(T lhs, T rhs) {
  return lhs == rhs;
}

template <typename T>
bool compare_less_equal(T lhs, T rhs) {
  return lhs <= rhs;
}

template <typename T>
bool compare_greater(T lhs, T rhs) {
  return lhs > rhs;
}

template <typename T>
bool compare_not_equal(T lhs, T rhs) {
  return lhs != rhs;
}

template <typename T>
bool compare_greater_equal(T lhs, T rhs) {
  return lhs >= rhs;
}

template <typename T>
bool compare_always(T /*lhs*/, T /*rhs*/) {
  return true;
}

uint32_t sop_keep(uint32_t /*ref*/, uint32_t cur_stencil) {
  return cur_stencil;
}

uint32_t sop_zero(uint32_t /*ref*/, uint32_t /*cur_stencil*/) {
  return 0;
}

uint32_t sop_replace(uint32_t ref, uint32_t /*cur_stencil*/) {
  return ref;
}

uint32_t sop_incr_sat(uint32_t /*ref*/, uint32_t cur_stencil) {
  return std::min<uint32_t>(0xFF, cur_stencil + 1);
}

uint32_t sop_decr_sat(uint32_t /*ref*/, uint32_t cur_stencil) {
  return std::max<uint32_t>(0, cur_stencil - 1);
}

uint32_t sop_invert(uint32_t /*ref*/, uint32_t cur_stencil) {
  return ~cur_stencil;
}

uint32_t sop_incr_wrap(uint32_t /*ref*/, uint32_t cur_stencil) {
  return (cur_stencil + 1) & 0xFF;
}

uint32_t sop_decr_wrap(uint32_t /*ref*/, uint32_t cur_stencil) {
  return (cur_stencil - 1 + 256) & 0xFF;
}

depth_stencil_state::depth_stencil_state(const depth_stencil_desc& desc) : desc_(desc) {
  if (desc.depth_enable) {
    switch (desc.depth_func) {
    case compare_function_never: depth_test_ = compare_never<float>; break;
    case compare_function_less: depth_test_ = compare_less<float>; break;
    case compare_function_equal: depth_test_ = compare_equal<float>; break;
    case compare_function_less_equal: depth_test_ = compare_less_equal<float>; break;
    case compare_function_greater: depth_test_ = compare_greater<float>; break;
    case compare_function_not_equal: depth_test_ = compare_not_equal<float>; break;
    case compare_function_greater_equal: depth_test_ = compare_greater_equal<float>; break;
    case compare_function_always: depth_test_ = compare_always<float>; break;
    }
  } else {
    depth_test_ = compare_always<float>;
  }

  if (desc.stencil_enable) {
    mask_stencil_ = mask_stencil_1;

    compare_function stencil_compare_functions[2] = {desc.front_face.stencil_func,
                                                     desc.back_face.stencil_func};

    for (int i = 0; i < 2; ++i) {
      switch (stencil_compare_functions[i]) {
      case compare_function_never: stencil_test_[i] = compare_never<uint32_t>; break;
      case compare_function_less: stencil_test_[i] = compare_less<uint32_t>; break;
      case compare_function_equal: stencil_test_[i] = compare_equal<uint32_t>; break;
      case compare_function_less_equal: stencil_test_[i] = compare_less_equal<uint32_t>; break;
      case compare_function_greater: stencil_test_[i] = compare_greater<uint32_t>; break;
      case compare_function_not_equal: stencil_test_[i] = compare_not_equal<uint32_t>; break;
      case compare_function_greater_equal:
        stencil_test_[i] = compare_greater_equal<uint32_t>;
        break;
      case compare_function_always: stencil_test_[i] = compare_always<uint32_t>; break;
      }
    }

    stencil_op sops[6] = {desc.front_face.stencil_fail_op,
                          desc.front_face.stencil_pass_op,
                          desc.front_face.stencil_depth_fail_op,
                          desc.back_face.stencil_fail_op,
                          desc.back_face.stencil_pass_op,
                          desc.back_face.stencil_depth_fail_op};

    for (int i = 0; i < 6; ++i) {
      switch (sops[i]) {
      case stencil_op_keep: stencil_op_[i] = sop_keep; break;
      case stencil_op_zero: stencil_op_[i] = sop_zero; break;
      case stencil_op_replace: stencil_op_[i] = sop_replace; break;
      case stencil_op_incr_sat: stencil_op_[i] = sop_incr_sat; break;
      case stencil_op_decr_sat: stencil_op_[i] = sop_decr_sat; break;
      case stencil_op_invert: stencil_op_[i] = sop_invert; break;
      case stencil_op_incr_wrap: stencil_op_[i] = sop_incr_wrap; break;
      case stencil_op_decr_wrap: stencil_op_[i] = sop_decr_wrap; break;
      }
    }
  } else {
    mask_stencil_ = mask_stencil_0;
    for (auto& test_fn : stencil_test_) {
      test_fn = compare_always<uint32_t>;
    }

    for (auto& op : stencil_op_) {
      op = sop_keep;
    }
  }
}

const depth_stencil_desc& depth_stencil_state::get_desc() const {
  return desc_;
}

uint32_t depth_stencil_state::mask_stencil(uint32_t stencil, uint32_t mask) const {
  return mask_stencil_(stencil, mask);
}

bool depth_stencil_state::depth_test(float ps_depth, float cur_depth) const {
  return depth_test_(ps_depth, cur_depth);
}

bool depth_stencil_state::stencil_test(bool front_face, uint32_t ref, uint32_t cur_stencil) const {
  return stencil_test_[!front_face](ref, cur_stencil);
}

uint32_t depth_stencil_state::stencil_operation(
    bool front_face, bool depth_pass, bool stencil_pass, uint32_t ref, uint32_t cur_stencil) const {
  return stencil_op_[(!front_face) * 3 + (!depth_pass) + static_cast<int>(stencil_pass)](
      ref, cur_stencil);
}

// frame buffer

void read_depth_0_stencil_0(float& depth,
                            uint32_t& stencil,
                            uint32_t /*stencil_mask*/,
                            void const* /*ds_data*/) {
  depth = 0.0f;
  stencil = 0;
}

template <uint32_t Format>
void read_depth_0_stencil_1(float& depth,
                            uint32_t& stencil,
                            uint32_t stencil_mask,
                            void const* ds_data) {
  depth = 0.0f;
  stencil = depth_stencil_accessor<Format>::read_stencil(ds_data) & stencil_mask;
}

template <uint32_t Format>
void read_depth_1_stencil_0(float& depth,
                            uint32_t& stencil,
                            uint32_t /*stencil_mask*/,
                            void const* ds_data) {
  depth = depth_stencil_accessor<Format>::read_depth(ds_data);
  stencil = 0;
}

template <uint32_t Format>
void read_depth_1_stencil_1(float& depth,
                            uint32_t& stencil,
                            uint32_t stencil_mask,
                            void const* ds_data) {
  depth_stencil_accessor<Format>::read_depth_stencil(depth, stencil, ds_data);
  stencil &= stencil_mask;
}

void write_depth_0_stencil_0(void* /*ds_data*/,
                             float /*depth*/,
                             uint32_t /*stencil*/,
                             uint32_t /*stencil_mask*/) {
}

template <uint32_t Format>
void write_depth_0_stencil_1(void* ds_data,
                             float /*depth*/,
                             uint32_t stencil,
                             uint32_t stencil_mask) {
  depth_stencil_accessor<Format>::write_stencil(ds_data, stencil & stencil_mask);
}

template <uint32_t Format>
void write_depth_1_stencil_0(void* ds_data,
                             float depth,
                             uint32_t /*stencil*/,
                             uint32_t /*stencil_mask*/) {
  depth_stencil_accessor<Format>::write_depth(ds_data, depth);
}

template <uint32_t Format>
void write_depth_1_stencil_1(void* ds_data, float depth, uint32_t stencil, uint32_t stencil_mask) {
  depth_stencil_accessor<Format>::write_depth_stencil(ds_data, depth, stencil & stencil_mask);
}

void framebuffer::initialize(render_stages const* /*stages*/) {
}

void framebuffer::update(render_state* state) {
  bool ds_state_changed = (ds_state_ != state->ds_state.get());
  bool ds_format_changed = true;
  if (ds_target_ != nullptr && state->depth_stencil_target.get() != nullptr &&
      state->depth_stencil_target->get_pixel_format() == ds_target_->get_pixel_format()) {
    ds_format_changed = false;
  }

  ds_state_ = state->ds_state.get();
  stencil_read_mask_ = ds_state_->get_desc().stencil_read_mask;
  stencil_write_mask_ = ds_state_->get_desc().stencil_write_mask;
  stencil_ref_ = ds_state_->mask_stencil(state->stencil_ref, stencil_read_mask_);

  assert(state->color_targets.size() <= MAX_RENDER_TARGETS);
  memset(color_targets_, 0, sizeof(color_targets_));
  for (size_t i = 0; i < state->color_targets.size(); ++i) {
    color_targets_[i] = state->color_targets[i].get();
  }

  ds_target_ = state->depth_stencil_target.get();
  sample_count_ = static_cast<uint32_t>(state->target_sample_count);
  px_full_mask_ = (1UL << sample_count_) - 1;

  bool output_depth_enabled = false;
  if (!state->vx_shader) {
    if (state->cpp_ps && state->cpp_ps->output_depth()) {
      output_depth_enabled = true;
    }
  }

  update_ds_rw_functions(ds_format_changed, ds_state_changed, output_depth_enabled);
}

void framebuffer::update_ds_rw_functions(bool ds_format_changed,
                                         bool ds_state_changed,
                                         bool output_depth_enabled) {
  if (!ds_format_changed && !ds_state_changed) {
    return;
  }

  read_depth_stencil_ = read_depth_0_stencil_0;
  write_depth_stencil_ = write_depth_0_stencil_0;

  if (ds_target_ == nullptr) {
    return;
  }

  bool read_depth = false;
  bool read_stencil = false;
  bool write_depth = false;
  bool write_stencil = false;

  switch (ds_target_->get_pixel_format()) {
  case pixel_format_color_rg32f:
    if (ds_state_->get_desc().depth_enable) {
      if (ds_state_->get_desc().depth_func != compare_function_never &&
          ds_state_->get_desc().depth_func != compare_function_always) {
        read_depth = true;
      }

      if (ds_state_->get_desc().depth_write_mask &&
          ds_state_->get_desc().depth_func != compare_function_never) {
        write_depth = true;
      }
    }

    read_stencil = write_stencil = ds_state_->get_desc().stencil_enable;

    if (read_depth) {
      if (read_stencil) {
        read_depth_stencil_ = read_depth_1_stencil_1<pixel_format_color_rg32f>;
      } else {
        read_depth_stencil_ = read_depth_1_stencil_0<pixel_format_color_rg32f>;
      }
    } else {
      if (read_stencil) {
        read_depth_stencil_ = read_depth_0_stencil_1<pixel_format_color_rg32f>;
      } else {
        read_depth_stencil_ = read_depth_0_stencil_0;
      }
    }

    if (write_depth) {
      if (write_stencil) {
        write_depth_stencil_ = write_depth_1_stencil_1<pixel_format_color_rg32f>;
      } else {
        write_depth_stencil_ = write_depth_1_stencil_0<pixel_format_color_rg32f>;
      }
    } else {
      if (write_stencil) {
        write_depth_stencil_ = write_depth_0_stencil_1<pixel_format_color_rg32f>;
      } else {
        write_depth_stencil_ = write_depth_0_stencil_0;
      }
    }
    break;
  default: return;
  }

  early_z_enabled_ = !ds_state_->get_desc().stencil_enable && !output_depth_enabled;
}

framebuffer::framebuffer() {
  for (size_t i = 0; i < MAX_RENDER_TARGETS; ++i) {
    color_targets_[i] = nullptr;
  }

  ds_target_ = nullptr;
  ds_state_ = nullptr;
  stencil_ref_ = 0;
  stencil_read_mask_ = 0;
  stencil_write_mask_ = 0;

  read_depth_stencil_ = nullptr;
  write_depth_stencil_ = nullptr;
}

framebuffer::~framebuffer() {
}

void framebuffer::render_sample(cpp_blend_shader* cpp_bs,
                                size_t x,
                                size_t y,
                                size_t i_sample,
                                const ps_output& ps,
                                float depth,
                                bool front_face) {
  EF_ASSERT(cpp_bs, "Blend shader is null or invalid.");
  if (!cpp_bs)
    return;

  // composing output
  pixel_accessor target_pixel(color_targets_, ds_target_);
  target_pixel.set_pos(x, y);

  if (early_z_enabled_) {
    cpp_bs->execute(i_sample, target_pixel, ps);
    return;
  }

  void* ds_data = target_pixel.depth_stencil_address(i_sample);
  float old_depth;
  uint32_t old_stencil;
  read_depth_stencil_(old_depth, old_stencil, stencil_read_mask_, ds_data);

  bool depth_passed = ds_state_->depth_test(depth, old_depth);
  bool stencil_passed = ds_state_->stencil_test(front_face, stencil_ref_, old_stencil);

  if (depth_passed && stencil_passed) {
    int32_t new_stencil = ds_state_->stencil_operation(
        front_face, depth_passed, stencil_passed, stencil_ref_, old_stencil);
    cpp_bs->execute(i_sample, target_pixel, ps);
    write_depth_stencil_(ds_data, depth, new_stencil, stencil_write_mask_);
  }
}

void framebuffer::render_sample_quad(cpp_blend_shader* cpp_bs,
                                     size_t x,
                                     size_t y,
                                     uint64_t sample_mask,
                                     ps_output const* quad,
                                     float const* depth,
                                     bool front_face,
                                     float const* aa_offset) {
  EF_ASSERT(cpp_bs, "Blend shader is null or invalid.");
  if (!cpp_bs)
    return;

  for (int i = 0; i < 4; ++i) {
    size_t pixel_x = x + (i & 1);
    size_t pixel_y = y + ((i & 2) >> 1);

    uint64_t px_sample_mask = sample_mask & SAMPLE_MASK;
    sample_mask >>= MAX_SAMPLE_COUNT;

    if (px_sample_mask == 0) {
      continue;
    }

    if (sample_count_ == 1) {
      render_sample(cpp_bs, pixel_x, pixel_y, 0, quad[i], depth[i], front_face);
    } else if (px_sample_mask == px_full_mask_) {
      for (uint32_t i_samp = 0; i_samp < sample_count_; ++i_samp) {
        render_sample(
            cpp_bs, pixel_x, pixel_y, i_samp, quad[i], depth[i] + aa_offset[i_samp], front_face);
      }
    } else {
      uint32_t i_samp;
      while (_xmm_bsf(&i_samp, (uint32_t)px_sample_mask)) {
        render_sample(
            cpp_bs, pixel_x, pixel_y, i_samp, quad[i], depth[i] + aa_offset[i_samp], front_face);
        px_sample_mask &= px_sample_mask - 1;
      }
    }
  }
}

uint64_t framebuffer::early_z_test(size_t x, size_t y, float depth, float const* aa_z_offset) {
  pixel_accessor target_pixel(color_targets_, ds_target_);
  target_pixel.set_pos(x, y);

  if (sample_count_ == 1) {
    void* ds_data = target_pixel.depth_stencil_address(0);
    float old_depth;
    uint32_t old_stencil;
    read_depth_stencil_(old_depth, old_stencil, stencil_read_mask_, ds_data);
    if (ds_state_->depth_test(depth, old_depth)) {
      assert(!ds_state_->get_desc().stencil_enable);
      write_depth_stencil_(ds_data, depth, 0, 0);
      return 1;
    }
    return 0;
  }

  uint64_t mask = 0;
  for (size_t i = 0; i < sample_count_; ++i) {
    void* ds_data = target_pixel.depth_stencil_address(i);
    float old_depth;
    uint32_t old_stencil;
    read_depth_stencil_(old_depth, old_stencil, stencil_read_mask_, ds_data);
    float new_depth = aa_z_offset[i] + depth;
    bool depth_test_passed = ds_state_->depth_test(new_depth, old_depth);
    mask |= (depth_test_passed ? 1 : 0) << i;
    if (depth_test_passed) {
      write_depth_stencil_(ds_data, new_depth, 0, 0);
    }
  }

  return mask;
}

uint64_t
framebuffer::early_z_test_quad(size_t x, size_t y, float const* depth, float const* aa_z_offset) {
  return (early_z_test(x + 0, y + 0, depth[0], aa_z_offset) << (MAX_SAMPLE_COUNT * 0)) |
      (early_z_test(x + 1, y + 0, depth[1], aa_z_offset) << (MAX_SAMPLE_COUNT * 1)) |
      (early_z_test(x + 0, y + 1, depth[2], aa_z_offset) << (MAX_SAMPLE_COUNT * 2)) |
      (early_z_test(x + 1, y + 1, depth[3], aa_z_offset) << (MAX_SAMPLE_COUNT * 3));
}

uint64_t framebuffer::early_z_test(
    size_t x, size_t y, uint32_t px_mask, float depth, float const* aa_z_offset) {
  if (px_mask == px_full_mask_) {
    return early_z_test(x, y, depth, aa_z_offset);
  }

  pixel_accessor target_pixel(color_targets_, ds_target_);
  target_pixel.set_pos(x, y);

  uint64_t mask = 0;
  uint32_t i_samp;
  while (_xmm_bsf(&i_samp, (uint32_t)px_mask)) {
    void* ds_data = target_pixel.depth_stencil_address(i_samp);
    float old_depth;
    uint32_t old_stencil;
    read_depth_stencil_(old_depth, old_stencil, stencil_read_mask_, ds_data);
    float new_depth = aa_z_offset[i_samp] + depth;
    bool depth_test_passed = ds_state_->depth_test(new_depth, old_depth);
    mask |= (depth_test_passed ? 1 : 0) << i_samp;
    px_mask &= (px_mask - 1);
    if (depth_test_passed) {
      write_depth_stencil_(ds_data, new_depth, 0, 0);
    }
  }

  return mask;
}

uint64_t framebuffer::early_z_test_quad(
    size_t x, size_t y, uint64_t quad_mask, float const* depth, float const* aa_z_offset) {
  uint32_t px_mask;

  uint64_t mask = 0;
  px_mask = static_cast<uint32_t>((quad_mask >> (MAX_SAMPLE_COUNT * 0)) & SAMPLE_MASK);
  mask |= (px_mask == 0 ? 0 : early_z_test(x + 0, y + 0, px_mask, depth[0], aa_z_offset))
      << (MAX_SAMPLE_COUNT * 0);

  px_mask = static_cast<uint32_t>((quad_mask >> (MAX_SAMPLE_COUNT * 1)) & SAMPLE_MASK);
  mask |= (px_mask == 0 ? 0 : early_z_test(x + 1, y + 0, px_mask, depth[1], aa_z_offset))
      << (MAX_SAMPLE_COUNT * 1);

  px_mask = static_cast<uint32_t>((quad_mask >> (MAX_SAMPLE_COUNT * 2)) & SAMPLE_MASK);
  mask |= (px_mask == 0 ? 0 : early_z_test(x + 0, y + 1, px_mask, depth[2], aa_z_offset))
      << (MAX_SAMPLE_COUNT * 2);

  px_mask = static_cast<uint32_t>((quad_mask >> (MAX_SAMPLE_COUNT * 3)) & SAMPLE_MASK);
  mask |= (px_mask == 0 ? 0 : early_z_test(x + 1, y + 1, px_mask, depth[3], aa_z_offset))
      << (MAX_SAMPLE_COUNT * 3);

  return mask;
}

void framebuffer::clear_depth_stencil(surface* tar, uint32_t flag, float depth, uint32_t stencil) {
  auto clear_op = write_depth_0_stencil_0;

  switch (tar->get_pixel_format()) {
  case pixel_format_color_rg32f: {
    switch (flag) {
    case (clear_depth | clear_stencil):
      clear_op = write_depth_1_stencil_1<pixel_format_color_rg32f>;
      break;
    case clear_depth: clear_op = write_depth_1_stencil_0<pixel_format_color_rg32f>; break;
    case clear_stencil: clear_op = write_depth_0_stencil_1<pixel_format_color_rg32f>; break;
    default: ef_unimplemented();
    }
    break;
  }
  default: ef_unimplemented();
  }

  for (size_t y = 0; y < tar->height(); ++y) {
    for (size_t x = 0; x < tar->width(); ++x) {
      pixel_accessor target_pixel(nullptr, tar);
      target_pixel.set_pos(x, y);
      for (size_t sample = 0; sample < tar->sample_count(); ++sample) {
        void* addr = target_pixel.depth_stencil_address(sample);
        clear_op(addr, depth, stencil, 0xFFFFFFFFU);
      }
    }
  }
}

}  // namespace salvia::core
