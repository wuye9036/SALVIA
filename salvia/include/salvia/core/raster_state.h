#pragma once

#include <salvia/common/constants.h>

#include <eflib/utility/shared_declaration.h>

#include <functional>
#include <memory>
#include <vector>

namespace salvia::shader {
class vs_output;
}

namespace salvia::core {

struct raster_desc {
  fill_mode fm;
  cull_mode cm;
  bool front_ccw;
  int32_t depth_bias;
  float depth_bias_clamp;
  float slope_scaled_depth_bias;
  bool depth_clip_enable;
  bool scissor_enable;
  bool multisample_enable;
  bool anti_aliased_line_enable;

  raster_desc()
    : fm(fill_solid)
    , cm(cull_back)
    , front_ccw(false)
    , depth_bias(0)
    , depth_bias_clamp(0)
    , slope_scaled_depth_bias(0)
    , depth_clip_enable(true)
    , scissor_enable(false)
    , multisample_enable(true)
    , anti_aliased_line_enable(false) {}
};

EFLIB_DECLARE_CLASS_SHARED_PTR(clipper);
EFLIB_DECLARE_CLASS_SHARED_PTR(cpp_pixel_shader);
EFLIB_DECLARE_CLASS_SHARED_PTR(pixel_shader_unit);

struct clip_context;
struct clip_results;
class rasterizer;
class raster_multi_prim_context;

class raster_state {
public:
  typedef bool (*cull_func)(float area);

private:
  raster_desc desc_;
  cull_func cull_;
  prim_type prim_;

public:
  raster_state(raster_desc const& desc);

  inline raster_desc const& get_desc() const { return desc_; }

  inline cull_func get_cull_func() const { return cull_; }

  inline bool cull(float area) const { return cull_(area); }
};

}  // namespace salvia::core
