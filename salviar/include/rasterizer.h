#pragma once

#include <salviar/include/salviar_forward.h>

#include <salviar/include/decl.h>
#include <salviar/include/enums.h>
#include <salviar/include/shader.h>
#include <salviar/include/framebuffer.h>
#include <salviar/include/raster_state.h>
#include <salviar/include/geom_setup_engine.h>
#include <salviar/include/async_object.h>

#include <eflib/include/memory/atomic.h>
#include <eflib/include/memory/pool.h>
#include <eflib/include/memory/allocator.h>

#include <array>
#include <functional>
#include <memory>

BEGIN_NS_SALVIAR();

typedef eflib::pool::reserved_pool<vs_output> vs_output_pool;

struct scanline_info;
class  pixel_shader_unit;
class  vs_output;
class  host;
struct vs_output_op;
struct clip_context;
class  shader_reflection;

struct pixel_statistic;
struct drawing_triangle_context;

template <typename T, int Alignment>
using aligned_vector = std::vector<T, eflib::aligned_allocator<T, Alignment>>;

struct drawing_shader_context
{
    cpp_pixel_shader*	cpp_ps;
	pixel_shader_unit*	ps_unit;
	cpp_blend_shader*	cpp_bs;
};

struct rasterize_multi_prim_context
{
	std::vector<uint32_t> const*	sorted_prims;
	viewport const*					tile_vp;
    pixel_statistic*                pixel_stat;
    drawing_shader_context          shaders;
};

struct rasterize_prim_context
{
	uint32_t						prim_id;
	viewport const*					tile_vp;
    pixel_statistic*                pixel_stat;
    drawing_shader_context          shaders;
};

class rasterizer
{
private:
	static const int MAX_NUM_MULTI_SAMPLES = 4;

	// Status per drawing.
	uint32_t						num_vs_output_attributes_;

	vertex_cache*					vert_cache_;
	host*							host_;
	framebuffer*					frame_buffer_;

	raster_state*					state_;
	pixel_shader_unit*				ps_proto_;
	cpp_vertex_shader*				cpp_vs_;
	cpp_pixel_shader*				cpp_ps_;
	cpp_blend_shader*				cpp_bs_;
	viewport const*					vp_;
    viewport const*                 target_vp_;
    size_t                          target_sample_count_;
	uint64_t						full_mask_;
	uint64_t						quad_full_mask_;
	vs_output_op const*				vso_ops_;
    bool                            has_centroid_;
    uint32_t                        prim_count_;
	bool							prim_reorderable_;		// Primitives could be reordered to rendering.

    async_object*                   pipeline_stat_;
    async_object*                   internal_stat_;
	async_object*					pipeline_prof_;

    accumulate_fn<uint64_t>::type   acc_ia_primitives_;
    accumulate_fn<uint64_t>::type   acc_cinvocations_;
    accumulate_fn<uint64_t>::type   acc_cprimitives_;
    accumulate_fn<uint64_t>::type   acc_ps_invocations_;
    accumulate_fn<uint64_t>::type   acc_backend_input_pixels_;

	time_stamp_fn::type				fetch_time_stamp_;
	accumulate_fn<uint64_t>::type	acc_vp_trans_;
	accumulate_fn<uint64_t>::type	acc_tri_dispatch_;
	accumulate_fn<uint64_t>::type	acc_ras_;
	accumulate_fn<uint64_t>::type	acc_clipping_;
	accumulate_fn<uint64_t>::type	acc_compact_clip_;

	// Intermediate data
	prim_type						prim_;
	uint32_t						prim_size_;
	eflib::vec2						samples_pattern_[MAX_NUM_MULTI_SAMPLES];

	std::vector<std::vector<std::vector<uint32_t>>>
									threaded_tiled_prims_;		// vector<prim> prims = thread_tiled_prims[ThreadID][TileID]
	aligned_vector<triangle_info, 16>
									tri_infos_;

	vs_output**						clipped_verts_;
	size_t							clipped_verts_count_;
	size_t							clipped_prims_count_;

	size_t							tile_x_count_;
	size_t							tile_y_count_;
	size_t							tile_count_;

	shader_reflection const*		vs_reflection_;

	std::vector<cpp_pixel_shader*>	threaded_cpp_ps_;
	std::vector<pixel_shader_unit*>	threaded_psu_;

	std::function< void (rasterizer*, rasterize_multi_prim_context*)>
									rasterize_prims_;
	geom_setup_engine				gse_;

	void threaded_dispatch_primitive(thread_context const*);
	void threaded_rasterize_multi_prim(thread_context const*);

	void draw_full_tile(
		int left, int top, int right, int bottom,
		drawing_shader_context const* shaders,
		drawing_triangle_context const* triangle_ctx);
	void draw_partial_tile(
		int left, int top,
		const eflib::vec4* edge_factors,
		drawing_shader_context const* shaders,
        drawing_triangle_context const* triangle_ctx);
	void subdivide_tile(
        int left, int top, const eflib::rect<uint32_t>& cur_region, const eflib::vec4* edge_factors,
		uint32_t* test_regions, uint32_t& test_region_size, float x_min, float x_max, float y_min, float y_max,
		const float* rej_to_acc, const float* evalue, const float* step_x, const float* step_y);

	void draw_full_quad(
		uint32_t left, uint32_t top,
		drawing_shader_context const* shaders, drawing_triangle_context const* triangle_ctx);
	void draw_quad(
		uint32_t left, uint32_t top, uint64_t quad_mask,
		drawing_shader_context const* shaders, drawing_triangle_context const* triangle_ctx);

	void viewport_and_project_transform(vs_output** vertexes, size_t num_verts);
	void compute_triangle_info(uint32_t prim_id);

	void prepare_draw();
public:
	//inherited
	void initialize	(render_stages const* stages);
	void update		(render_state const* state);

	//constructor
	rasterizer();
	~rasterizer();

	//drawer
	void rasterize_line(rasterize_prim_context const*);
	void rasterize_triangle(rasterize_prim_context const*);

	void rasterize_multi_line(rasterize_multi_prim_context const*);
	void rasterize_multi_triangle(rasterize_multi_prim_context const*);

	void draw();

	void update_prim_info(render_state const* state);
};

END_NS_SALVIAR();
