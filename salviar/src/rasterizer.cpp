#include <salviar/include/rasterizer.h>

#include <salviar/include/clipper.h>
#include <salviar/include/framebuffer.h>
#include <salviar/include/host.h>
#include <salviar/include/render_state.h>
#include <salviar/include/render_stages.h>
#include <salviar/include/shader_reflection.h>
#include <salviar/include/shader_object.h>
#include <salviar/include/shader_unit.h>
#include <salviar/include/shader_regs.h>
#include <salviar/include/shader_regs_op.h>
#include <salviar/include/thread_pool.h>
#include <salviar/include/thread_context.h>
#include <salviar/include/vertex_cache.h>

#include <eflib/include/diagnostics/log.h>
#include <eflib/include/platform/cpuinfo.h>
#include <eflib/include/platform/intrin.h>
#include <eflib/include/utility/unref_declarator.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/format.hpp>
#include <eflib/include/platform/boost_end.h>

#include <algorithm>

#if defined(EFLIB_MSVC)
#include <ppl.h>
#endif

using eflib::num_available_threads;

using std::atomic;

class shader_reflection;

BEGIN_NS_SALVIAR();

using namespace std;
using namespace eflib;
using namespace boost;

int const TILE_SIZE = 64;
int const DISPATCH_PRIMITIVE_PACKAGE_SIZE = 8;
int const VP_PROJ_TRANSFORM_PAKCAGE_SIZE = 8;
int const RASTERIZE_PRIMITIVE_PACKAGE_SIZE = 1;

#define DEBUG_QUAD 0
#if DEBUG_QUAD
bool is_valid_quad(size_t quad_x, size_t quad_y)
{
	size_t debug_start_x = 0;
	size_t debug_start_y = 0;
	size_t width = 512;
	size_t height = 512;

	return 
		debug_start_x <= quad_x && quad_x < debug_start_x + width &&
		debug_start_y <= quad_y && quad_y < debug_start_y + height;
}
#endif

struct pixel_statistic
{
    uint64_t ps_invocations;
    uint64_t backend_input_pixels;
};

struct drawing_triangle_context
{
    float const*			aa_z_offset;
	triangle_info const*	tri_info;
    pixel_statistic*		pixel_stat;
};

/*************************************************
 *   Steps for line rasterization��
 *			1 Find major direction and computing distance and differential on major direction.
 *			2 Calculate ddx and ddy for mip-mapping.
 *			3 Computing pixel position and interpolated attribute by DDA with major direction and differential.
 *			4 Executing pixel shader
 *			5 Render pixel into framebuffer.
 *
 *   Note:
 *			1 Position is in window coordinate.
 *			2 x y z components of w-pos have been divided by w component.
 *			3 positon.w() = 1.0f / clip w
 **************************************************/
void rasterizer::rasterize_line(rasterize_prim_context const* ctx)
{
	// Extract to local variables
    cpp_pixel_shader*	cpp_ps	= ctx->shaders.cpp_ps;
    pixel_shader_unit*	psu		= ctx->shaders.ps_unit;
	viewport  const&	vp		= *ctx->tile_vp;
	vs_output const&	v0		= *clipped_verts_[ctx->prim_id*2+0];
	vs_output const&	v1		= *clipped_verts_[ctx->prim_id*2+1];

	// Rasterize
	vs_output diff;
	vso_ops_->sub(diff, v1, v0);
	eflib::vec4 const& dir = diff.position();
	float diff_dir = abs(dir.x()) > abs(dir.y()) ? dir.x() : dir.y();

	// Computing differential.
	vs_output ddx, ddy;
	vso_ops_->mul(ddx, diff, (diff.position().x() / (diff.position().xy().length_sqr())));
	vso_ops_->mul(ddy, diff, (diff.position().y() / (diff.position().xy().length_sqr())));

	int vpleft  = fast_floori( max(0.0f, vp.x) );
	int vptop   = fast_floori( max(0.0f, vp.y) );
    int vpright = fast_floori( min(vp.x+vp.w, target_vp_->w) );
    int vpbottom= fast_floori( min(vp.y+vp.h, target_vp_->h) );

	ps_output px_out;

	// Divided drawing to x major DDA method and y major DDA method.
	if( abs(dir.x()) > abs(dir.y()))
	{
		//Swap start and end to make diff is positive.
		const vs_output *start, *end;
		if(dir.x() < 0){
			start = &v1;
			end = &v0;
			diff_dir = -diff_dir;
		} else {
			start = &v0;
			end = &v1;
		}

		EFLIB_ASSERT_UNIMPLEMENTED();

		float fsx = fast_floor(start->position().x() + 0.5f);

		int sx = fast_floori(fsx);
		int ex = fast_floori(end->position().x() - 0.5f);

		// Clamp to visible screen.
		sx = eflib::clamp<int>(sx, vpleft, int(vpright - 1));
		ex = eflib::clamp<int>(ex, vpleft, int(vpright));

		// Set attributes of start point.
		vs_output px_start, px_end;
		vso_ops_->copy(px_start, *start);
		vso_ops_->copy(px_end, *end);
		float step = sx + 0.5f - start->position().x();
		vs_output px_in;
		vso_ops_->lerp(px_in, px_start, px_end, step / diff_dir);

		// Draw line with x major DDA.
		vs_output unprojed;
		for(int iPixel = sx; iPixel < ex; ++iPixel)
		{
			// Ingore pixels which are outside of viewport.
			if(px_in.position().y() >= vpbottom){
				if(dir.y() > 0) break;
				continue;
			}
			if(px_in.position().y() < 0){
				if(dir.y() < 0) break;
				continue;
			}

			// Render pixel.
			vso_ops_->unproject(unprojed, px_in);

#if 0
			if(cpp_ps->execute(unprojed, px_out))
			{
				frame_buffer_->render_sample(cpp_bs_, iPixel, fast_floori(px_in.position().y()), 0, px_out, px_out.depth);
			}
#endif
			// Increment ddx
			++ step;
			vso_ops_->lerp(px_in, px_start, px_end, step / diff_dir);
		}
	}
	else //y major
	{
		const vs_output *start, *end;
		if(dir.y() < 0){
			start = &v1;
			end = &v0;
			diff_dir = -diff_dir;
		} else {
			start = &v0;
			end = &v1;
		}

		EFLIB_ASSERT_UNIMPLEMENTED();
		
		float fsy = fast_floor(start->position().y() + 0.5f);

		int sy = fast_floori(fsy);
		int ey = fast_floori(end->position().y() - 0.5f);

		sy = eflib::clamp<int>(sy, vptop, int(vpbottom - 1));
		ey = eflib::clamp<int>(ey, vptop, int(vpbottom));

		vs_output px_start, px_end;
		vso_ops_->copy(px_start, *start);
		vso_ops_->copy(px_end, *end);
		float step = sy + 0.5f - start->position().y();
		vs_output px_in;
		vso_ops_->lerp(px_in, px_start, px_end, step / diff_dir);

		vs_output unprojed;
		for(int iPixel = sy; iPixel < ey; ++iPixel)
		{
			if(px_in.position().x() >= vpright){
				if(dir.x() > 0) break;
				continue;
			}
			if(px_in.position().x() < 0){
				if(dir.x() < 0) break;
				continue;
			}

			vso_ops_->unproject(unprojed, px_in);

#if 0
			if(cpp_ps->execute(unprojed, px_out))
			{
				frame_buffer_->render_sample(cpp_bs_, fast_floori(px_in.position().x()), iPixel, 0, px_out, px_out.depth);
			}
#endif
			++ step;
			vso_ops_->lerp(px_in, px_start, px_end, step / diff_dir);
		}
	}
}

void rasterizer::initialize(render_stages const* stages)
{
	frame_buffer_	= stages->backend.get();
	vert_cache_		= stages->vert_cache.get();
	host_			= stages->host.get();
}

void rasterizer::update(render_state const* state)
{
	state_		            = state->ras_state.get();
	ps_proto_	            = state->ps_proto.get();
	cpp_vs_					= state->cpp_vs.get();
	cpp_ps_		            = state->cpp_ps.get();
	cpp_bs_		            = state->cpp_bs.get();
	vp_			            = &(state->vp);
    target_vp_              = &(state->target_vp);
    target_sample_count_    = state->target_sample_count;
	full_mask_				= (1ULL << target_sample_count_) - 1;
	quad_full_mask_			= 
		( full_mask_ << (MAX_SAMPLE_COUNT * 0) ) |
		( full_mask_ << (MAX_SAMPLE_COUNT * 1) ) |
		( full_mask_ << (MAX_SAMPLE_COUNT * 2) ) |
		( full_mask_ << (MAX_SAMPLE_COUNT * 3) );
	prim_reorderable_		= false;
	
	vs_reflection_ = state->vx_shader ? state->vx_shader->get_reflection() : nullptr;

	update_prim_info(state);

    // Initialize statistics.
    pipeline_stat_ = state->asyncs[static_cast<uint32_t>(async_object_ids::pipeline_statistics)].get();
    internal_stat_ = state->asyncs[static_cast<uint32_t>(async_object_ids::internal_statistics)].get();
	pipeline_prof_ = state->asyncs[static_cast<uint32_t>(async_object_ids::pipeline_profiles)].get();

    if(pipeline_stat_)
    {
        acc_ia_primitives_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::ia_primitives>;
        acc_cinvocations_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::cinvocations>;
        acc_cprimitives_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::cprimitives>;
        acc_ps_invocations_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::ps_invocations>;
    }
    else
    {
        acc_ia_primitives_ = &accumulate_fn<uint64_t>::null;
        acc_cprimitives_ = &accumulate_fn<uint64_t>::null;
        acc_cinvocations_ = &accumulate_fn<uint64_t>::null;
        acc_ps_invocations_ = &accumulate_fn<uint64_t>::null;
    }

    if(internal_stat_)
    {
        acc_backend_input_pixels_ = &async_internal_statistics::accumulate<internal_statistics_id::backend_input_pixels>;
    }
    else
    {
        acc_backend_input_pixels_ = &accumulate_fn<uint64_t>::null;
    }

	if(pipeline_prof_)
	{
		fetch_time_stamp_	= &async_pipeline_profiles::time_stamp;
		acc_vp_trans_		= &async_pipeline_profiles::accumulate<pipeline_profile_id::vp_trans>;
		acc_tri_dispatch_	= &async_pipeline_profiles::accumulate<pipeline_profile_id::tri_dispatch>;
		acc_ras_			= &async_pipeline_profiles::accumulate<pipeline_profile_id::ras>;
		acc_clipping_		= &async_pipeline_profiles::accumulate<pipeline_profile_id::clipping>;
		acc_compact_clip_	= &async_pipeline_profiles::accumulate<pipeline_profile_id::compact_clip>;
	}
	else
	{
		fetch_time_stamp_	= &time_stamp_fn::null;
		acc_clipping_		= &accumulate_fn<uint64_t>::null;
		acc_compact_clip_   = &accumulate_fn<uint64_t>::null;
		acc_vp_trans_		= &accumulate_fn<uint64_t>::null;
		acc_tri_dispatch_	= &accumulate_fn<uint64_t>::null;
		acc_ras_			= &accumulate_fn<uint64_t>::null;
	}
}

void rasterizer::draw_full_tile(
	int tile_left, int tile_top, int tile_right, int tile_bottom,
	drawing_shader_context const* shaders,
	drawing_triangle_context const* triangle_ctx)
{
	for(int top = tile_top; top < tile_bottom; top += 2)
	{
		for(int left = tile_left; left < tile_right; left += 2)
		{
			draw_full_quad(left, top, shaders, triangle_ctx);
		}
	}
}

void rasterizer::draw_partial_tile(
	int left, int top,
	const eflib::vec4* edge_factors,
	drawing_shader_context const* shaders,
    drawing_triangle_context const* triangle_ctx)
{
    auto& v0 = *triangle_ctx->tri_info->v0;

	const uint32_t full_mask = (1UL << target_sample_count_) - 1;

#ifndef EFLIB_NO_SIMD
	const __m128 mtx = _mm_set_ps(1, 0, 1, 0);
	const __m128 mty = _mm_set_ps(1, 1, 0, 0);

	__m128 medge0 = _mm_load_ps(&edge_factors[0].x());
	__m128 medge1 = _mm_load_ps(&edge_factors[1].x());
	__m128 medge2 = _mm_load_ps(&edge_factors[2].x());

	__m128 mtmp = _mm_unpacklo_ps(medge0, medge1);
	__m128 medgex = _mm_shuffle_ps(mtmp, medge2, _MM_SHUFFLE(3, 0, 1, 0));
	__m128 medgey = _mm_shuffle_ps(mtmp, medge2, _MM_SHUFFLE(3, 1, 3, 2));
	mtmp = _mm_unpackhi_ps(medge0, medge1);
	__m128 medgez = _mm_shuffle_ps(mtmp, medge2, _MM_SHUFFLE(3, 2, 1, 0));

	__m128 mleft = _mm_set1_ps(left);
	__m128 mtop = _mm_set1_ps(top);
	__m128 mevalue3 = _mm_sub_ps(medgez, _mm_add_ps(_mm_mul_ps(mleft, medgex), _mm_mul_ps(mtop, medgey)));
#endif

	EFLIB_ALIGN(16) uint32_t pixel_mask[4 * 4];

#if !defined(EFLIB_NO_SIMD)
	__m128i zero = _mm_setzero_si128();
	_mm_store_si128(reinterpret_cast<__m128i*>(pixel_mask + 0), zero);
	_mm_store_si128(reinterpret_cast<__m128i*>(pixel_mask + 4), zero);
	_mm_store_si128(reinterpret_cast<__m128i*>(pixel_mask + 8), zero);
	_mm_store_si128(reinterpret_cast<__m128i*>(pixel_mask + 12), zero);
#else
	memset(pixel_mask, 0, sizeof(pixel_mask));
#endif

#if !defined(EFLIB_NO_SIMD)
	for (size_t i_sample = 0; i_sample < target_sample_count_; ++ i_sample)
	{
		const vec2& sp = samples_pattern_[i_sample];
		__m128 mspx = _mm_set1_ps(sp.x());
		__m128 mspy = _mm_set1_ps(sp.y());

		for(int iy = 0; iy < 4; ++ iy)
		{
			__m128 my = _mm_add_ps(mspy, _mm_set1_ps(iy));
			__m128 mx = _mm_add_ps(mspx, _mm_set_ps(3, 2, 1, 0));
			
			__m128 mask_rej = _mm_setzero_ps();
			{
				__m128 mstepx = _mm_shuffle_ps(medgex, medgex, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 mstepy = _mm_shuffle_ps(medgey, medgey, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 msteprej = _mm_add_ps(_mm_mul_ps(mx, mstepx), _mm_mul_ps(my, mstepy));

				__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(0, 0, 0, 0));

				mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			}
			{
				__m128 mstepx = _mm_shuffle_ps(medgex, medgex, _MM_SHUFFLE(1, 1, 1, 1));
				__m128 mstepy = _mm_shuffle_ps(medgey, medgey, _MM_SHUFFLE(1, 1, 1, 1));
				__m128 msteprej = _mm_add_ps(_mm_mul_ps(mx, mstepx), _mm_mul_ps(my, mstepy));

				__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(1, 1, 1, 1));

				mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			}
			{
				__m128 mstepx = _mm_shuffle_ps(medgex, medgex, _MM_SHUFFLE(2, 2, 2, 2));
				__m128 mstepy = _mm_shuffle_ps(medgey, medgey, _MM_SHUFFLE(2, 2, 2, 2));
				__m128 msteprej = _mm_add_ps(_mm_mul_ps(mx, mstepx), _mm_mul_ps(my, mstepy));

				__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(2, 2, 2, 2));

				mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			}

			__m128 sample_mask = _mm_castsi128_ps(_mm_set1_epi32(1UL << i_sample));
			sample_mask = _mm_andnot_ps(mask_rej, sample_mask);

			__m128 stored_mask = _mm_load_ps( reinterpret_cast<float*>(pixel_mask + iy * 4) );
			stored_mask = _mm_or_ps(sample_mask, stored_mask);
			_mm_store_ps(reinterpret_cast<float*>(pixel_mask + iy * 4), stored_mask);
		}
	}
#else
	float evalue[3];
	for (int e = 0; e < 3; ++ e)
	{
		evalue[e] = edge_factors[e].z() - (left * edge_factors[e].x() + top * edge_factors[e].y());
	}

	for(int iy = 0; iy < 4; ++iy)
	{
		// Rasterizer.
		for(size_t ix = 0; ix < 4; ++ix)
		{
			for (int i_sample = 0; i_sample < target_sample_count_; ++ i_sample)
			{
				vec2  const& sp = samples_pattern_[i_sample];
				float const  fx = ix + sp.x();
				float const  fy = iy + sp.y();
				bool inside = true;
				for (int e = 0; e < 3; ++ e)
				{
					if (fx * edge_factors[e].x() + fy * edge_factors[e].y() < evalue[e])
					{
						inside = false;
						break;
					}
				}

				if (inside)
				{
					pixel_mask[iy * 4 + ix] |= 1UL << i_sample;
				}
			}
		}
	}
#endif

    for(int quad = 0; quad < 4; ++quad)
    {
		int const quad_x = (quad & 1) << 1;
		int const quad_y = (quad & 2);

		int const quad_start = quad_x | (quad_y << 2);

		uint64_t quad_mask = 
			( (pixel_mask[quad_start+0] & static_cast<uint64_t>(SAMPLE_MASK)) << (MAX_SAMPLE_COUNT * 0) ) |
			( (pixel_mask[quad_start+1] & static_cast<uint64_t>(SAMPLE_MASK)) << (MAX_SAMPLE_COUNT * 1) ) |
			( (pixel_mask[quad_start+4] & static_cast<uint64_t>(SAMPLE_MASK)) << (MAX_SAMPLE_COUNT * 2) ) |
			( (pixel_mask[quad_start+5] & static_cast<uint64_t>(SAMPLE_MASK)) << (MAX_SAMPLE_COUNT * 3) );

		// No sample need to render.
        if(quad_mask == 0)
        {
            continue;
        }

		if (quad_mask == quad_full_mask_)
		{
			draw_full_quad(left + quad_x, top + quad_y, shaders, triangle_ctx);
		}
		else
		{
			draw_quad(left + quad_x, top + quad_y, quad_mask, shaders, triangle_ctx);
		}
    }
}

void rasterizer::subdivide_tile(
    int left, int top,
    const eflib::rect<uint32_t>& cur_region,
	const vec4* edge_factors,
    uint32_t* test_regions, uint32_t& test_region_size,
    float x_min, float x_max, float y_min, float y_max,
	const float* rej_to_acc, const float* evalue,
    const float* step_x, const float* step_y)
{
	const uint32_t new_w = cur_region.w;
	const uint32_t new_h = cur_region.h;

#ifndef EFLIB_NO_SIMD
	static const union
	{
		int maski;
		float maskf;
	} MASK = { 0x80000000 };
	static const __m128 SIGN_MASK = _mm_set1_ps(MASK.maskf);

	__m128 medge0 = _mm_load_ps(&edge_factors[0].x());
	__m128 medge1 = _mm_load_ps(&edge_factors[1].x());
	__m128 medge2 = _mm_load_ps(&edge_factors[2].x());

	__m128 mtmp = _mm_unpacklo_ps(medge0, medge1);
	__m128 medgex = _mm_shuffle_ps(mtmp, medge2, _MM_SHUFFLE(3, 0, 1, 0));
	__m128 medgey = _mm_shuffle_ps(mtmp, medge2, _MM_SHUFFLE(3, 1, 3, 2));
	mtmp = _mm_unpackhi_ps(medge0, medge1);

	__m128 mstepx3 = _mm_load_ps(step_x);
	__m128 mstepy3 = _mm_load_ps(step_y);
	__m128 mrej2acc3 = _mm_load_ps(rej_to_acc);

	__m128 mleft = _mm_set1_ps(left);
	__m128 mtop = _mm_set1_ps(top);
	__m128 mevalue3 = _mm_sub_ps(_mm_load_ps(evalue), _mm_add_ps(_mm_mul_ps(mleft, medgex), _mm_mul_ps(mtop, medgey)));

	for(int iy = 0; iy < 4; ++ iy){
		__m128 my = _mm_mul_ps(mstepy3, _mm_set1_ps(iy));

		__m128 mask_rej = _mm_setzero_ps();
		__m128 mask_acc = _mm_setzero_ps();

		// Trival rejection & acception
		{
			__m128 mstepx = _mm_mul_ps(_mm_shuffle_ps(mstepx3, mstepx3, _MM_SHUFFLE(0, 0, 0, 0)), _mm_set_ps(3, 2, 1, 0));
			__m128 mstepy = _mm_shuffle_ps(my, my, _MM_SHUFFLE(0, 0, 0, 0));

			__m128 mrej2acc = _mm_shuffle_ps(mrej2acc3, mrej2acc3, _MM_SHUFFLE(0, 0, 0, 0));

			__m128 msteprej = _mm_add_ps(mstepx, mstepy);
			__m128 mstepacc = _mm_add_ps(msteprej, mrej2acc);

			__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(0, 0, 0, 0));

			mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			mask_acc = _mm_or_ps(mask_acc, _mm_cmplt_ps(mstepacc, mevalue));
		}
		{
			__m128 mstepx = _mm_mul_ps(_mm_shuffle_ps(mstepx3, mstepx3, _MM_SHUFFLE(1, 1, 1, 1)), _mm_set_ps(3, 2, 1, 0));
			__m128 mstepy = _mm_shuffle_ps(my, my, _MM_SHUFFLE(1, 1, 1, 1));

			__m128 mrej2acc = _mm_shuffle_ps(mrej2acc3, mrej2acc3, _MM_SHUFFLE(1, 1, 1, 1));

			__m128 msteprej = _mm_add_ps(mstepx, mstepy);
			__m128 mstepacc = _mm_add_ps(msteprej, mrej2acc);

			__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(1, 1, 1, 1));

			mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			mask_acc = _mm_or_ps(mask_acc, _mm_cmplt_ps(mstepacc, mevalue));
		}
		{
			__m128 mstepx = _mm_mul_ps(_mm_shuffle_ps(mstepx3, mstepx3, _MM_SHUFFLE(2, 2, 2, 2)), _mm_set_ps(3, 2, 1, 0));
			__m128 mstepy = _mm_shuffle_ps(my, my, _MM_SHUFFLE(2, 2, 2, 2));

			__m128 mrej2acc = _mm_shuffle_ps(mrej2acc3, mrej2acc3, _MM_SHUFFLE(2, 2, 2, 2));

			__m128 msteprej = _mm_add_ps(mstepx, mstepy);
			__m128 mstepacc = _mm_add_ps(msteprej, mrej2acc);

			__m128 mevalue = _mm_shuffle_ps(mevalue3, mevalue3, _MM_SHUFFLE(2, 2, 2, 2));

			mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
			mask_acc = _mm_or_ps(mask_acc, _mm_cmplt_ps(mstepacc, mevalue));
		}
		mask_acc = _mm_andnot_ps(mask_acc, SIGN_MASK);

		__m128i mineww = _mm_set1_epi32(new_w);
		__m128i minewh = _mm_set1_epi32(new_h);
		__m128i mix = _mm_set_epi32(3 * new_w, 2 * new_w, 1 * new_w, 0 * new_h);
		__m128i miy = _mm_set1_epi32(iy * new_h);
		mix = _mm_add_epi32(mix, _mm_set1_epi32(cur_region.x));
		miy = _mm_add_epi32(miy, _mm_set1_epi32(cur_region.y));
		__m128i miregion = _mm_or_si128(mix, _mm_slli_epi32(miy, 8));
		miregion = _mm_or_si128(miregion, _mm_castps_si128(mask_acc));

		EFLIB_ALIGN(16) uint32_t region_code[4];
		_mm_store_si128(reinterpret_cast<__m128i*>(&region_code[0]), miregion);

		mask_rej = _mm_or_ps(mask_rej, _mm_cmpge_ps(_mm_set1_ps(x_min), _mm_cvtepi32_ps(_mm_add_epi32(mix, mineww))));
		mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(_mm_set1_ps(x_max), _mm_cvtepi32_ps(mix)));
		mask_rej = _mm_or_ps(mask_rej, _mm_cmpge_ps(_mm_set1_ps(y_min), _mm_cvtepi32_ps(_mm_add_epi32(miy, minewh))));
		mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(_mm_set1_ps(y_max), _mm_cvtepi32_ps(miy)));

		int rejections = ~_mm_movemask_ps(mask_rej) & 0xF;
		uint32_t t;
		while (_xmm_bsf(&t, (uint32_t)rejections))
		{
			assert(t < 4);

			test_regions[test_region_size] = region_code[t];
			++ test_region_size;

			rejections &= rejections - 1;
		}
	}
#else
	float evalue1[3];
	for (int e = 0; e < 3; ++ e){
		evalue1[e] = evalue[e] - (left * edge_factors[e].x() + top * edge_factors[e].y());
	}

	for (int ty = 0; ty < 4; ++ ty){
		uint32_t y = cur_region.y + new_h * ty;
		for (int tx = 0; tx < 4; ++ tx){
			uint32_t x = cur_region.x + new_w * tx;

			if ((x_min < x + new_w) && (x_max >= x)
				&& (y_min < y + new_h) && (y_max >= y))
			{
				int rejection = 0;
				int acception = 1;

				// Trivial rejection & acception
				for (int e = 0; e < 3; ++ e){
					float step = tx * step_x[e] + ty * step_y[e];
					rejection |= (step < evalue1[e]);
					acception &= (step + rej_to_acc[e] >= evalue1[e]);
				}

				if (!rejection){
					test_regions[test_region_size] = x + (y << 8) + (acception << 31);
					++ test_region_size;
				}
			}
		}
	}
#endif
}

/*************************************************
*   Steps of triangle rasterization��
*			1 Generate scan line and compute derivation of scanlines
*			2 Rasterize scan line by rasterizer_scanline_impl
*			3 Generate vs_output/ps_input for pixels.
*			4 Execute pixel shader
*			5 Render pixel to frame buffer.
*
*   Note:
*			1 All of position pixel is in window coordinate system.
*			2 x, y, z components of wpos have been devided by 'clip w'.
*			3 positon.w() == 1.0f / 'clip w'
**************************************************/
void rasterizer::rasterize_triangle(rasterize_prim_context const* ctx)
{
	// Extract to local variables
    cpp_pixel_shader*	cpp_ps			= ctx->shaders.cpp_ps;
    pixel_shader_unit*	psu				= ctx->shaders.ps_unit;
	viewport  const&	vp				= *ctx->tile_vp;
	uint32_t			prim_id			= ctx->prim_id >> 1;
	uint32_t			full			= ctx->prim_id & 1;
	vs_output const&	v0				= *clipped_verts_[prim_id*3+0];
	vs_output const&	v1				= *clipped_verts_[prim_id*3+1];
	vs_output const&	v2				= *clipped_verts_[prim_id*3+2];

	triangle_info const* tri_info     = tri_infos_.data() + prim_id;
	eflib::vec4 const*	 edge_factors = tri_info->edge_factors;
	bool const mark_x[3] = 
	{
		edge_factors[0].x() > 0, edge_factors[1].x() > 0, edge_factors[2].x() > 0
	};
	bool const mark_y[3] = 
	{
		edge_factors[0].y() > 0, edge_factors[1].y() > 0, edge_factors[2].y() > 0
	};

	enum TRI_VS_TILE 
	{
		TVT_FULL,
		TVT_PARTIAL,
		TVT_EMPTY,
		TVT_PIXEL
	};

	float const x_min = tri_info->bounding_box[0] - vp.x;
	float const x_max = tri_info->bounding_box[1] - vp.x;
	float const y_min = tri_info->bounding_box[2] - vp.y;
	float const y_max = tri_info->bounding_box[3] - vp.y;

	/*************************************************
	*   Draw triangles with Larrabee algorithm .
	*************************************************/

	uint32_t test_regions[2][TILE_SIZE / 2 * TILE_SIZE / 2];
	uint32_t test_region_size[2] = { 0, 0 };
	test_regions[0][0] = (full << 31);
	test_region_size[0] = 1;
	int src_stage = 0;
	int dst_stage = !src_stage;

	const int vpleft0 = fast_floori(vp.x);
	const int vpright0 = fast_floori(vp.x + vp.w);
	const int vptop0 = fast_floori(vp.y);
	const int vpbottom0 = fast_floori(vp.y + vp.h);

	uint32_t subtile_w = fast_floori(vp.w);
	uint32_t subtile_h = fast_floori(vp.h);

	EFLIB_ALIGN(16) float step_x[4];
	EFLIB_ALIGN(16) float step_y[4];
	EFLIB_ALIGN(16) float rej_to_acc[4];
	EFLIB_ALIGN(16) float evalue[4];
	float part_evalue[4];
	for (int e = 0; e < 3; ++ e)
	{
		step_x[e] = TILE_SIZE * edge_factors[e].x();
		step_y[e] = TILE_SIZE * edge_factors[e].y();
		rej_to_acc[e] = -abs(step_x[e]) - abs(step_y[e]);
		part_evalue[e] = mark_x[e] * TILE_SIZE * edge_factors[e].x() + mark_y[e] * TILE_SIZE * edge_factors[e].y();
		evalue[e] = edge_factors[e].z() - part_evalue[e];
	}
	step_x[3] = step_y[3] = 0;

	float aa_z_offset[MAX_NUM_MULTI_SAMPLES];
	if (target_sample_count_ > 1)
	{
		for (unsigned long i_sample = 0; i_sample < target_sample_count_; ++ i_sample)
		{
			const vec2& sp = samples_pattern_[i_sample];
			aa_z_offset[i_sample] = (sp.x() - 0.5f) * tri_info->ddx.position().z() + (sp.y() - 0.5f) * tri_info->ddy.position().z();
		}
	}
    else
    {
        aa_z_offset[0] = 0.0f;
    }

    drawing_triangle_context tri_ctx;
    tri_ctx.aa_z_offset = aa_z_offset;
    tri_ctx.pixel_stat  = ctx->pixel_stat;
	tri_ctx.tri_info	= tri_info;
	if (cpp_ps != nullptr)
	{
		cpp_ps->update_front_face(tri_info->front_face);
	}

	float target_vp_right = target_vp_->w + target_vp_->x;
	float target_vp_bottom = target_vp_->h +  + target_vp_->y;

	while (test_region_size[src_stage] > 0)
	{
		test_region_size[dst_stage] = 0;

		subtile_w /= 4;
		subtile_h /= 4;

		for (int e = 0; e < 3; ++ e)
		{
			step_x[e] *= 0.25f;
			step_y[e] *= 0.25f;
			rej_to_acc[e] *= 0.25f;
			part_evalue[e] *= 0.25f;
			evalue[e] = edge_factors[e].z() - part_evalue[e];
		}

		for (size_t ivp = 0; ivp < test_region_size[src_stage]; ++ ivp)
		{
			const uint32_t packed_region = test_regions[src_stage][ivp];
			eflib::rect<uint32_t> cur_region(
				packed_region & 0xFF, 
				(packed_region >> 8) & 0xFF,
				subtile_w, subtile_h);
			TRI_VS_TILE intersect = (packed_region >> 31) ? TVT_FULL : TVT_PARTIAL;

			const int vpleft = max(0U, static_cast<unsigned>(vpleft0 + cur_region.x) );
			const int vptop  = max(0U, static_cast<unsigned>(vptop0 + cur_region.y) );

			// Sub tile is out of screen.
			if (vpleft >= target_vp_right || vptop >= target_vp_bottom)
			{
				continue;
			}

			// For one pixel region
			if ((TVT_PARTIAL == intersect) && (cur_region.w <= 1) && (cur_region.h <= 1))
			{
				intersect = TVT_PIXEL;
			}

			switch (intersect)
			{
			case TVT_EMPTY:
				// Empty tile. Do nothing.
				break;

			case TVT_FULL:
				{
					// The whole tile is inside a triangle.
					const int vpright  = min( vpleft0 + cur_region.x + cur_region.w * 4, static_cast<uint32_t>(target_vp_right)  );
					const int vpbottom = min( vptop0  + cur_region.y + cur_region.h * 4, static_cast<uint32_t>(target_vp_bottom) );
					this->draw_full_tile(vpleft, vptop, vpright, vpbottom, &ctx->shaders, &tri_ctx);
				}
				break;

			case TVT_PIXEL:
				// The tile is small enough for pixel level matching.
				this->draw_partial_tile(vpleft, vptop, edge_factors, &ctx->shaders, &tri_ctx);
				break;

			default:
				// Only a part of the triangle is inside the tile. So subdivide the tile into small ones.
				this->subdivide_tile(
					vpleft, vptop,
					cur_region, edge_factors,
					test_regions[dst_stage], test_region_size[dst_stage],
					x_min, x_max, y_min, y_max,
					rej_to_acc, evalue, step_x, step_y);
				break;
			}
		}

		src_stage = (src_stage + 1) & 1;
		dst_stage = !src_stage;
	}
}

rasterizer::rasterizer()
{
}

rasterizer::~rasterizer()
{
}

void rasterizer::threaded_dispatch_primitive(thread_context const* thread_ctx)
{
	vector<vector<uint32_t>>& tiled_prims = threaded_tiled_prims_[thread_ctx->thread_id];
	for(auto& prims: tiled_prims)
	{
		prims.clear();
	}

	thread_context::package_cursor current_package = thread_ctx->next_package();
	while ( current_package.valid() )
	{
		auto prim_range = current_package.item_range();

		for (int32_t i = prim_range.first; i < prim_range.second; ++ i)
		{
			if (3 == prim_size_)
			{
				compute_triangle_info(i);
			}

			triangle_info const* tri_info = tri_infos_.data() + i;
			
			if (tri_info->v0 == nullptr)
			{
				continue;
			}
			float const x_min = tri_info->bounding_box[0];
			float const x_max = tri_info->bounding_box[1];
			float const y_min = tri_info->bounding_box[2];
			float const y_max = tri_info->bounding_box[3];

			const int sx = std::min(fast_floori(std::max(0.0f, x_min) / TILE_SIZE),		static_cast<int>(tile_x_count_));
			const int sy = std::min(fast_floori(std::max(0.0f, y_min) / TILE_SIZE),		static_cast<int>(tile_y_count_));
			const int ex = std::min(fast_ceili (std::max(0.0f, x_max) / TILE_SIZE) + 1,	static_cast<int>(tile_x_count_));
			const int ey = std::min(fast_ceili (std::max(0.0f, y_max) / TILE_SIZE) + 1,	static_cast<int>(tile_y_count_));

			if ((sx + 1 == ex) && (sy + 1 == ey))
			{
				// Small primitive
				tiled_prims[sy * tile_x_count_ + sx].push_back(i << 1);
			}
			else
			{
				if (3 == prim_size_)
				{
					vec4 const* edge_factors = tri_infos_[i].edge_factors;

					bool const mark_x[3] =
					{
						edge_factors[0].x() > 0, edge_factors[1].x() > 0, edge_factors[2].x() > 0
					};

					bool const mark_y[3] =
					{
						edge_factors[0].y() > 0, edge_factors[1].y() > 0, edge_factors[2].y() > 0
					};

					float step_x[3];
					float step_y[3];
					float rej_to_acc[3];
					for (int e = 0; e < 3; ++ e)
					{
						step_x[e] = TILE_SIZE * edge_factors[e].x();
						step_y[e] = TILE_SIZE * edge_factors[e].y();
						rej_to_acc[e] = -abs(step_x[e]) - abs(step_y[e]);
					}

					for (int y = sy; y < ey; ++ y)
					{
						for (int x = sx; x < ex; ++ x)
						{
							int rejection = 0;
							int acception = 1;

							// Trival rejection & acception
							for (int e = 0; e < 3; ++ e)
							{
								float evalue = edge_factors[e].z() - ((x + mark_x[e]) * TILE_SIZE * edge_factors[e].x() + (y + mark_y[e]) * TILE_SIZE * edge_factors[e].y());
								rejection |= (0 < evalue);
								acception &= (rej_to_acc[e] >= evalue);
							}

							if (!rejection)
							{
								tiled_prims[y * tile_x_count_ + x].push_back((i << 1) | acception);
							}
						}
					}
				}
				else
				{
					for (int y = sy; y < ey; ++ y)
					{
						for (int x = sx; x < ex; ++ x)
						{
							tiled_prims[y * tile_x_count_ + x].push_back(i << 1);
						}
					}
				}
			}
		}

		current_package = thread_ctx->next_package();
	}
}

void rasterizer::compute_triangle_info(uint32_t i)
{
	triangle_info* tri_info = tri_infos_.data() + i;
	tri_info->v0 = nullptr;

	vs_output const* verts[3] =
	{
		clipped_verts_[i*prim_size_+0],
		clipped_verts_[i*prim_size_+1],
		clipped_verts_[i*prim_size_+2],
	};

	vec4 const* vert_pos[3] =
	{
		&( verts[0]->position() ),
		&( verts[1]->position() ),
		&( verts[2]->position() )
	};

	// Reorder vertexes.
	// Pick the vertex which is nearby center of viewport
	// It will get more precision in interpolation.
	double dist[3] =
	{
		fabs( vert_pos[0]->x() ) + fabs( vert_pos[0]->y() ),
		fabs( vert_pos[1]->x() ) + fabs( vert_pos[1]->y() ),
		fabs( vert_pos[2]->x() ) + fabs( vert_pos[2]->y() )
	};

	int reordered_index[3];
	if(dist[0] < dist[1])
	{
		if(dist[0] < dist[2])
		{
			reordered_index[0] = 0;
		}
		else
		{
			reordered_index[0] = 2;
		}
	}
	else
	{
		if(dist[1] < dist[2])
		{
			reordered_index[0] = 1;
		}
		else
		{
			reordered_index[0] = 2;
		}
	}
	reordered_index[1] = ( reordered_index[0] + 1 ) % 3;
	reordered_index[2] = ( reordered_index[1] + 1 ) % 3;

	vs_output const* reordered_verts[3] = { verts[reordered_index[0]], verts[reordered_index[1]], verts[reordered_index[2]] };

	// Compute difference along edge.
	vs_output e01, e02;
	vso_ops_->sub(e01, *reordered_verts[1], *reordered_verts[0]);
	vso_ops_->sub(e02, *reordered_verts[2], *reordered_verts[0]);

	// Compute area of triangle.
	float area = cross_prod2(e02.position().xy(), e01.position().xy());
	// Return for zero-area triangle.
	if( equal<float>(area, 0.0f) ) return;

	tri_info->front_face = area > 0.0f;
	float inv_area = 1.0f / area;

	// Compute bounding box
	tri_info->bounding_box[0] = std::min( std::min( vert_pos[0]->x(), vert_pos[1]->x() ), vert_pos[2]->x() );	// xmin
	tri_info->bounding_box[1] = std::max( std::max( vert_pos[0]->x(), vert_pos[1]->x() ), vert_pos[2]->x() );	// xmax
	tri_info->bounding_box[2] = std::min( std::min( vert_pos[0]->y(), vert_pos[1]->y() ), vert_pos[2]->y() );	// ymin
	tri_info->bounding_box[3] = std::max( std::max( vert_pos[0]->y(), vert_pos[1]->y() ), vert_pos[2]->y() );	// ymax

	for (int i_vert = 0; i_vert < 3; ++ i_vert)
	{
		// Edge factors: x * (y1 - y0) - y * (x1 - x0) - (y1 * x0 - x1 * y0)
		int const se = i_vert;
		int const ee = (i_vert + 1) % 3;

		vec4* edge_factors = tri_info->edge_factors;
		edge_factors[i_vert].x(vert_pos[se]->y() - vert_pos[ee]->y()										);
		edge_factors[i_vert].y(vert_pos[ee]->x() - vert_pos[se]->x()										);
		edge_factors[i_vert].z(vert_pos[ee]->x() * vert_pos[se]->y() - vert_pos[ee]->y() * vert_pos[se]->x());
		edge_factors[i_vert].w(0.0f);
	}

	// Compute difference of attributes.
	{
		vso_ops_->compute_derivative(tri_info->ddx, tri_info->ddy, e01, e02, inv_area);
	}

	tri_info->v0 = reordered_verts[0];
}

void rasterizer::threaded_rasterize_multi_prim(thread_context const* thread_ctx)
{
	viewport tile_vp;
	tile_vp.w = TILE_SIZE;
	tile_vp.h = TILE_SIZE;
	tile_vp.minz = vp_->minz;
	tile_vp.maxz = vp_->maxz;

	std::vector<uint32_t> prims;
    pixel_statistic pixel_stat;
    pixel_stat.ps_invocations = 0;
    pixel_stat.backend_input_pixels = 0;

	rasterize_multi_prim_context rast_ctxt;
    rast_ctxt.shaders.cpp_ps	= threaded_cpp_ps_[thread_ctx->thread_id];
    rast_ctxt.shaders.ps_unit	= threaded_psu_[thread_ctx->thread_id];
    rast_ctxt.shaders.cpp_bs    = cpp_bs_;
	rast_ctxt.tile_vp		    = &tile_vp;
	rast_ctxt.sorted_prims	    = &prims;
    rast_ctxt.pixel_stat        = &pixel_stat;

	thread_context::package_cursor current_package = thread_ctx->next_package();
	while ( current_package.valid() )
	{
		auto tile_range = current_package.item_range();
		for (int32_t i = tile_range.first; i < tile_range.second; ++ i)
		{
			prims.clear();
			for (size_t j = 0; j < threaded_tiled_prims_.size(); ++ j)
			{
				prims.insert(prims.end(), threaded_tiled_prims_[j][i].begin(), threaded_tiled_prims_[j][i].end());
			}
			std::sort(prims.begin(), prims.end());

			int y = i / tile_x_count_;
			int x = i - y * tile_x_count_;

			tile_vp.x = static_cast<float>(x * TILE_SIZE);
			tile_vp.y = static_cast<float>(y * TILE_SIZE);

			rast_ctxt.sorted_prims = &prims;

			rasterize_prims_(this, &rast_ctxt);

			current_package = thread_ctx->next_package();
		}
	}

    acc_ps_invocations_(pipeline_stat_, pixel_stat.ps_invocations);
    acc_backend_input_pixels_(internal_stat_, pixel_stat.backend_input_pixels);
}

void rasterizer::rasterize_multi_line(rasterize_multi_prim_context const* ctx)
{
	rasterize_prim_context prim_ctxt;
    prim_ctxt.shaders	= ctx->shaders;
	prim_ctxt.tile_vp	= ctx->tile_vp;

	for (uint32_t prim_with_mask: *ctx->sorted_prims)
	{
		prim_ctxt.prim_id = prim_with_mask >> 1;
		rasterize_line(&prim_ctxt);
	}
}

void rasterizer::rasterize_multi_triangle(rasterize_multi_prim_context const* ctx)
{
	rasterize_prim_context prim_ctxt;
    prim_ctxt.shaders   = ctx->shaders;
	prim_ctxt.tile_vp	= ctx->tile_vp;
    prim_ctxt.pixel_stat= ctx->pixel_stat;

	for (uint32_t prim_with_mask: *ctx->sorted_prims)
	{
		prim_ctxt.prim_id = prim_with_mask;
		rasterize_triangle(&prim_ctxt);
	}
}

void rasterizer::update_prim_info(render_state const* state)
{
	bool is_tri = false;
	bool is_line = false;
	bool is_point = false;

	bool is_wireframe = false;
	bool is_solid = false;

    prim_count_ = state->prim_count;

	switch (state_->get_desc().fm)
	{
	case fill_solid:
		is_solid = true;
		break;
	case fill_wireframe:
		is_wireframe = true;
		break;
	}

	switch (state->prim_topo)
	{
	case primitive_point_list:
	case primitive_point_sprite:
		is_point = false;
		break;
	case primitive_line_list:
	case primitive_line_strip:
		is_line = false;
		break;
	case primitive_triangle_list:
	case primitive_triangle_fan:
	case primitive_triangle_strip:
		is_tri = true;
		break;
	}

	if (is_solid && is_tri)
	{
		prim_ = pt_solid_tri;
	}
	else if (is_wireframe && is_tri)
	{
		prim_ = pt_wireframe_tri;
	}
	else
	{
		prim_ = pt_none;
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	switch(prim_)
	{
	case pt_line:
	case pt_wireframe_tri:
		prim_size_ = 2;
		break;
	case pt_solid_tri:
		prim_size_ = 3;
		break;
	default:
		EFLIB_ASSERT_UNIMPLEMENTED();
	}
}

void rasterizer::prepare_draw()
{
	// Set shader and interpolation attributes
	if(cpp_vs_)
	{
		num_vs_output_attributes_ = cpp_vs_->num_output_attributes();
		auto vso_op = &get_vs_output_op(num_vs_output_attributes_);
		for(uint32_t i = 0; i < num_vs_output_attributes_; ++i)
		{
			vso_op->attribute_modifiers[i] = cpp_vs_->output_attribute_modifiers(i);
		}
		vso_ops_ = vso_op;
	}
	else if(host_)
	{
		num_vs_output_attributes_ = host_->vs_output_attr_count();
		auto vso_op = &get_vs_output_op(num_vs_output_attributes_);
		for(uint32_t i = 0; i < num_vs_output_attributes_; ++i)
		{
			vso_op->attribute_modifiers[i] = vs_output::am_linear;
		}
		vso_ops_ = vso_op;
	}

    has_centroid_ = false;
   	for(size_t i_attr = 0; i_attr < num_vs_output_attributes_; ++i_attr)
	{
		if (vso_ops_->attribute_modifiers[i_attr] & vs_output::am_centroid)
		{
			has_centroid_ = true;
		}
	}

	// Compute sample patterns
	switch (target_sample_count_)
    {
	case 1:
		samples_pattern_[0] = vec2(0.5f, 0.5f);
		break;

	case 2:
		samples_pattern_[0] = vec2(0.25f, 0.25f);
		samples_pattern_[1] = vec2(0.75f, 0.75f);
		break;

	case 4:
		samples_pattern_[0] = vec2(0.375f, 0.125f);
		samples_pattern_[1] = vec2(0.875f, 0.375f);
		samples_pattern_[2] = vec2(0.125f, 0.625f);
		samples_pattern_[3] = vec2(0.625f, 0.875f);
		break;

	default:
		break;
	}

	// Compute tile count
	tile_x_count_	= static_cast<size_t>(vp_->w + TILE_SIZE - 1) / TILE_SIZE;
	tile_y_count_	= static_cast<size_t>(vp_->h + TILE_SIZE - 1) / TILE_SIZE;
	tile_count_		= tile_x_count_ * tile_y_count_;
}

void rasterizer::draw()
{
	vert_cache_->prepare_vertices();
	prepare_draw();

	size_t num_threads	= num_available_threads();

	geom_setup_context	geom_setup_ctx;

	geom_setup_ctx.cull			    = state_->get_cull_func();
	geom_setup_ctx.dvc			    = vert_cache_;
	geom_setup_ctx.prim			    = prim_;
	geom_setup_ctx.prim_count	    = prim_count_;
	geom_setup_ctx.prim_size	    = prim_size_;
	geom_setup_ctx.vso_ops		    = vso_ops_;
    geom_setup_ctx.pipeline_stat    = pipeline_stat_;
    geom_setup_ctx.acc_cinvocations = acc_cinvocations_;

	uint64_t clipping_start_time = fetch_time_stamp_();
	gse_.execute(&geom_setup_ctx, fetch_time_stamp_);
	acc_clipping_(pipeline_prof_, gse_.compact_start_time() - clipping_start_time);
	acc_compact_clip_(pipeline_prof_, fetch_time_stamp_() - gse_.compact_start_time());

	clipped_verts_			= gse_.verts();
	clipped_verts_count_	= gse_.verts_count();
	clipped_prims_count_	= clipped_verts_count_ / prim_size_;

    acc_ia_primitives_(pipeline_stat_, prim_count_);
    acc_cprimitives_(pipeline_stat_, clipped_prims_count_);

	// Project and Transformed to Viewport
	uint64_t vp_trans_start_time = fetch_time_stamp_();
	viewport_and_project_transform(clipped_verts_, clipped_verts_count_);
	acc_vp_trans_(pipeline_prof_, fetch_time_stamp_() - vp_trans_start_time);

	uint64_t tri_dispatch_start_time = fetch_time_stamp_();
	// Dispatch primitives into tiles' bucket
	threaded_tiled_prims_.resize(num_threads);
	tri_infos_.resize(clipped_prims_count_);
	for (size_t i = 0; i < num_threads; ++ i)
	{
		threaded_tiled_prims_[i].resize(tile_count_);
	}

	// Execute dispatching primitive
	execute_threads(
		[this](thread_context const* thread_ctx) { this->threaded_dispatch_primitive(thread_ctx); },
		clipped_prims_count_, DISPATCH_PRIMITIVE_PACKAGE_SIZE, num_threads
		);
	acc_tri_dispatch_(pipeline_prof_, fetch_time_stamp_() - tri_dispatch_start_time);

	// Rasterize tiles
	switch(prim_)
	{
	case pt_line:
	case pt_wireframe_tri:
		rasterize_prims_ = std::mem_fn(&rasterizer::rasterize_multi_line);
		break;
	case pt_solid_tri:
		rasterize_prims_ = std::mem_fn(&rasterizer::rasterize_multi_triangle);
		break;
	default:
		EFLIB_ASSERT(false, "Primitive type is not correct.");
	}

	std::vector<cpp_pixel_shader_ptr>	ppps(num_threads);
	std::vector<pixel_shader_unit_ptr>	ppsu(num_threads);

	threaded_cpp_ps_.resize(num_threads);
	threaded_psu_.resize(num_threads);

	for (size_t i = 0; i < num_threads; ++ i)
	{
		// create cpp_pixel_shader clone per thread from hps
		if(cpp_ps_ != nullptr)
		{
			ppps[i] = cpp_ps_->clone<cpp_pixel_shader>();
			threaded_cpp_ps_[i] = ppps[i].get();
		}
		if(ps_proto_ != nullptr)
		{
			ppsu[i] = ps_proto_->clone();
			threaded_psu_[i] = ppsu[i].get();
		}
	}

	uint64_t ras_start_time = fetch_time_stamp_();
	execute_threads(
		[this](thread_context const* thread_ctx){ this->threaded_rasterize_multi_prim(thread_ctx); },
		tile_count_, RASTERIZE_PRIMITIVE_PACKAGE_SIZE, num_threads
		);
	acc_ras_(pipeline_prof_, fetch_time_stamp_() - ras_start_time);

	// destroy all cpp_pixel_shader clone
	for (size_t i = 0; i < num_threads; ++ i)
	{
        ppps[i].reset();
	}
}

void threaded_viewport_and_project_transform(
	vs_output_functions::project proj_fn,
	vs_output** verts,
	viewport const* vp,
	thread_context const* thread_ctx)
{
	thread_context::package_cursor current_package = thread_ctx->next_package();
	while ( current_package.valid() )
	{
		auto prim_range = current_package.item_range();

		for (int32_t i = prim_range.first; i < prim_range.second; ++ i)
		{
			vs_output* vso = verts[i];
			viewport_transform(vso->position(), *vp);
			proj_fn(*vso, *vso);
		}
		current_package = thread_ctx->next_package();
	}
}

void rasterizer::viewport_and_project_transform(vs_output** vertexes, size_t num_verts)
{
	vs_output_functions::project proj_fn = vso_ops_->project;

	// Gathering vs_output need to be processed.
	vector<vs_output*> sorted;
	sorted.insert( sorted.end(), vertexes, vertexes+num_verts );

#if defined(EFLIB_MSVC)
	concurrency::parallel_sort( sorted.data(), sorted.data() + sorted.size() );
#else
	std::sort( sorted.begin(), sorted.end() );
#endif

	sorted.erase( std::unique(sorted.begin(), sorted.end()), sorted.end() );

	vs_output** trans_proj_verts = sorted.data();

	execute_threads(
		[this, trans_proj_verts](thread_context const* thread_ctx)
		{
			threaded_viewport_and_project_transform(this->vso_ops_->project, trans_proj_verts, this->vp_, thread_ctx);
		},
		static_cast<int>( sorted.size() ), VP_PROJ_TRANSFORM_PAKCAGE_SIZE
	);
}

void rasterizer::draw_full_quad(
	uint32_t left, uint32_t top,
	drawing_shader_context const* shaders,
    drawing_triangle_context const* triangle_ctx
	)
{
#if DEBUG_QUAD
	if (!is_valid_quad(left, top)) return;
#endif

#if 1
	EFLIB_ALIGN(16) vs_output pixels[4];

	float const dx = 0.5f + left - triangle_ctx->tri_info->v0->position().x();
	float const dy = 0.5f + top  - triangle_ctx->tri_info->v0->position().y();

	vso_ops_->step_2d_unproj_pos_quad(
		pixels, *triangle_ctx->tri_info->v0, 
		dx, triangle_ctx->tri_info->ddx,
		dy, triangle_ctx->tri_info->ddy
		);

	uint64_t  quad_mask = quad_full_mask_;
	ps_output pso[4];
	float     depth[4] = 
	{
		pixels[0].position().z(),
		pixels[1].position().z(),
		pixels[2].position().z(),
		pixels[3].position().z()
	};

	if ( frame_buffer_->early_z_enabled() )
	{
		quad_mask = frame_buffer_->early_z_test_quad(left, top, depth, triangle_ctx->aa_z_offset);
	}

	if (quad_mask == 0)
	{
		return;
	}
	
	triangle_ctx->pixel_stat->ps_invocations += 4;

	vso_ops_->step_2d_unproj_attr_quad(
		pixels, *triangle_ctx->tri_info->v0, 
		dx, triangle_ctx->tri_info->ddx,
		dy, triangle_ctx->tri_info->ddy
		);
	          
#if 0
	for(int i = 0; i < 4; ++i)
	{
		printf("%f_%f_%f_%f\n",
			unprojed[i].attribute(0).data_[0],
			unprojed[i].attribute(0).data_[1],
			unprojed[i].attribute(0).data_[2],
			unprojed[i].attribute(0).data_[3]
		);
	}
	printf("\n");
#endif

	if(shaders->ps_unit)
	{
		shaders->ps_unit->update(pixels, vs_reflection_);
		shaders->ps_unit->execute(pso, depth);
	}
	else
	{
		quad_mask &= shaders->cpp_ps->execute(pixels, pso, depth);
	}

	if(quad_mask != 0)
	{
		triangle_ctx->pixel_stat->backend_input_pixels += 4;
		frame_buffer_->render_sample_quad(
			shaders->cpp_bs, left, top, quad_mask,
			pso, depth, triangle_ctx->tri_info->front_face, triangle_ctx->aa_z_offset
			);
	}
#endif
}

void rasterizer::draw_quad(
	uint32_t left, uint32_t top, uint64_t quad_mask,
	drawing_shader_context const* shaders,
	drawing_triangle_context const* triangle_ctx)
{
#if DEBUG_QUAD
	if (!is_valid_quad(left, top)) return;
#endif

#if 1
	EFLIB_ALIGN(16) vs_output pixels[4];

	auto v0  =  triangle_ctx->tri_info->v0;
	auto ddx = &triangle_ctx->tri_info->ddx; 
	auto ddy = &triangle_ctx->tri_info->ddy;

	float const quad_dx = 0.5f + left - v0->position().x();
	float const quad_dy = 0.5f + top  - v0->position().y();

	vso_ops_->step_2d_unproj_pos_quad(pixels, *v0, quad_dx, *ddx, quad_dy, *ddy);

	ps_output pso[4];
	float     depth[4] = 
	{
		pixels[0].position().z(),
		pixels[1].position().z(),
		pixels[2].position().z(),
		pixels[3].position().z()
	};

	uint64_t tested_quad_mask = quad_mask;
	if ( frame_buffer_->early_z_enabled() )
	{
		tested_quad_mask = frame_buffer_->early_z_test_quad(left, top, quad_mask, depth, triangle_ctx->aa_z_offset);
	}

	if(tested_quad_mask == 0)
	{
		return;
	}

	if(!has_centroid_)
	{
		vso_ops_->step_2d_unproj_attr_quad(pixels, *v0, quad_dx, *ddx, quad_dy, *ddy);
	}
	else
	{
		for(int i_pixel = 0; i_pixel < 4; ++i_pixel)
		{
			int ix = (i_pixel & 1);
			int iy = (i_pixel & 2) >> 1;

			float dx = quad_dx + ix;
			float dy = quad_dy + iy;

			uint32_t pixel_mask = ( quad_mask >> (i_pixel * MAX_SAMPLE_COUNT) ) & SAMPLE_MASK;

			if(pixel_mask != full_mask_ && pixel_mask != 0)
			{
				vec2 sp_centroid(0, 0);
				int n = 0;

				uint32_t	   i_sample;
				uint32_t const mask_backup = pixel_mask;
				while (_xmm_bsf(&i_sample, pixel_mask))
				{
					const vec2& sp = samples_pattern_[i_sample];
					sp_centroid.x() += sp.x();
					sp_centroid.y() += sp.y();
					++ n;

					pixel_mask &= pixel_mask - 1;
				}
				sp_centroid /= n;

				pixel_mask = mask_backup;

				dx += sp_centroid.x() - 0.5f;
				dy += sp_centroid.y() - 0.5f;
			}
			vso_ops_->step_2d_unproj_attr(pixels[i_pixel], *v0, dx, *ddx, dy, *ddy);
		}
	}

	triangle_ctx->pixel_stat->ps_invocations += 4;

	if(shaders->ps_unit)
	{
		shaders->ps_unit->update(pixels, vs_reflection_);
		shaders->ps_unit->execute(pso, depth);
	}
	else
	{
		tested_quad_mask &= shaders->cpp_ps->execute(pixels, pso, depth);
	}

	if(quad_mask != 0)
	{
		triangle_ctx->pixel_stat->backend_input_pixels += 4;
		frame_buffer_->render_sample_quad(
			shaders->cpp_bs, left, top, tested_quad_mask,
			pso, depth, triangle_ctx->tri_info->front_face, triangle_ctx->aa_z_offset
			);
	}
#endif
}

END_NS_SALVIAR();
