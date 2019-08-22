#pragma once

#include <salviar/include/salviar_forward.h>

#include <eflib/include/platform/typedefs.h>
#include <eflib/include/string/string.h>

#include <string>

BEGIN_NS_SALVIAR();

enum class result: uint32_t
{
	ok,
	failed,
	outofmemory,
	invalid_parameter
};

enum map_mode
{
	map_mode_none = 0,
	map_read = 1,
	map_write = 2,
	map_read_write = 3,
	map_write_discard = 4,
	map_write_no_overwrite = 5
};

enum map_result
{
	map_succeed,
	map_failed,
	map_do_not_wait
};

enum resource_usage
{
	resource_access_none= 0x0,
	
	resource_read		= 0x1,	// xxx1
	resource_write		= 0x2, 	// xx1x
	
	resource_client		= 0x4,	// x1xx
	resource_device		= 0x8,	// 1xxx
	
	client_read			= 0x5,	// 0101
	client_write		= 0x6,	// 0110
	
	device_read			= 0x9,	// 1001
	device_write		= 0xa,	// 1010

	resource_default	= device_read | device_write,
	resource_immutable	= device_read | client_read,
	resource_dynamic	= device_read | client_write,
	resource_staging	= 0xf	// 1111
};

#define RESERVED(i) 0xFFFF0000 + i
enum primitive_topology
{
	primitive_point_list = RESERVED(0),
	primitive_point_sprite = RESERVED(1),
	
	primitive_line_list = 0,
	primitive_line_strip = 1,
	
	primitive_triangle_list = 2,
	primitive_triangle_fan = 3,
	primitive_triangle_strip = 4,

	primivite_topology_count = 5
};

enum cull_mode
{
	cull_none = 0,
	cull_front = 1,
	cull_back = 2,

	cull_mode_count = 3,
};

enum fill_mode
{
	fill_wireframe = 0,
	fill_solid = 1,

	fill_mode_count = 2
};

enum prim_type
{
	pt_none,
	pt_point,
	pt_line,
	pt_solid_tri,
	pt_wireframe_tri
};

enum texture_type
{
	texture_type_1d = 0,
	texture_type_2d = 1,
	texture_type_cube = 2,
	texture_type_count = 3,
};

enum address_mode
{
	address_wrap = 0,
	address_mirror = 1,
	address_clamp = 2,
	address_border = 3,
	address_mode_count = 4
};

enum filter_type
{
	filter_point = 0,
	filter_linear = 1,
	filter_anisotropic = 2,
	filter_type_count = 3
};

enum sampler_state
{
	sampler_state_min = 0,
	sampler_state_mag = 1,
	sampler_state_mip = 2,
	sampler_state_count = 3
};

enum sampler_axis
{
	sampler_axis_u = 0,
	sampler_axis_v = 1,
	sampler_axis_w = 2,
	sampler_axis_count = 3
};

enum cubemap_faces
{
	cubemap_face_positive_x = 0,
	cubemap_face_negative_x = 1,
	cubemap_face_positive_y = 2,
	cubemap_face_negative_y = 3,
	cubemap_face_positive_z = 4,
	cubemap_face_negative_z = 5,
	cubemap_faces_count = 6
};

// Usage describes the default component value will be filled to unfilled component if source data isn't a 4-components vector.
// Position means fill to (0, 0, 0, 1)
// Attrib means fill to (0,0,0,0)
enum input_register_usage_decl
{
	input_register_usage_position = 0,
	input_register_usage_attribute = 1,
	input_register_usage_decl_count = 2
};

enum render_target
{
    render_target_none = 0,
	render_target_color = 1,
	render_target_depth_stencil = 2,
	render_target_count = 3
};

enum compare_function
{
	compare_function_never = 0,
	compare_function_less = 1,
	compare_function_equal = 2,
	compare_function_less_equal = 3,
	compare_function_greater = 4,
	compare_function_not_equal = 5,
	compare_function_greater_equal = 6,
	compare_function_always = 7	
};

enum stencil_op
{
    stencil_op_keep = 1,
    stencil_op_zero = 2,
    stencil_op_replace = 3,
    stencil_op_incr_sat = 4,
    stencil_op_decr_sat = 5,
    stencil_op_invert = 6,
    stencil_op_incr_wrap = 7,
    stencil_op_decr_wrap = 8,
};

enum clear_flag
{
	clear_depth = 0x1,
	clear_stencil = 0x2
};

enum class async_object_ids: uint32_t
{
    none,
    event,
    occlusion,
    pipeline_statistics,
    occlusion_predicate,
    internal_statistics,
	pipeline_profiles,
    count
};

enum class async_status: uint32_t
{
    error,
    timeout,
    ready
};

END_NS_SALVIAR();
