#include <salviar/include/stream_assembler.h>

#include <salviar/include/input_layout.h>
#include <salviar/include/shader_regs.h>
#include <salviar/include/shader_regs_op.h>
#include <salviar/include/buffer.h>
#include <salviar/include/shader.h>
#include <salviar/include/render_state.h>
#include <salviar/include/stream_state.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/range/iterator_range.hpp>
#include <eflib/include/platform/boost_end.h>

using namespace eflib;

using std::make_tuple;
using std::get;
using std::tuple;

using std::vector;
using std::pair;
using std::make_pair;

BEGIN_NS_SALVIAR();

vec4 get_vec4(format fmt, float default_wcomp, const void* data)
{
	assert( data );

	if( !data ){ return vec4::zero(); }

	const float* floats = (const float*)data;

	switch(fmt)
	{
		case format_r32_float:
			return vec4(floats[0], 0.0f, 0.0f, default_wcomp);
		case format_r32g32_float:
			return vec4(floats[0], floats[1], 0.0f, default_wcomp);
		case format_r32g32b32_float:
			return vec4(floats[0], floats[1], floats[2], default_wcomp);
		case format_r32g32b32a32_float:
			return vec4(floats[0], floats[1], floats[2], floats[3]);
		case format_r32g32b32a32_sint:
			return *reinterpret_cast<vec4 const*>(floats);
		default:
			assert(false);
	}

	return vec4::zero();
}

void stream_assembler::update(render_state const* state)
{
	layout_				= state->layout.get();
	stream_buffer_descs_= state->str_state.buffer_descs.data();

	if(state->cpp_vs)
	{
		update_register_map(state->cpp_vs->get_register_map());
	}
}

/// Only used by Cpp Vertex Shader
void stream_assembler::update_register_map( std::unordered_map<semantic_value, size_t> const& reg_map )
{
	reg_ied_extra_.clear();
	reg_ied_extra_.reserve( reg_map.size() );

	typedef pair<semantic_value, size_t> pair_t;
	for(auto const& sv_reg_pair: reg_map)
	{
		input_element_desc const* elem_desc = layout_->find_desc(sv_reg_pair.first);
		
		if(elem_desc == nullptr)
		{
			reg_ied_extra_.clear();
			return;
		}

		reg_ied_extra_.push_back(reg_ied_extra_t{
			sv_reg_pair.second,
			elem_desc,
			semantic_value(elem_desc->semantic_name, elem_desc->semantic_index).default_wcomp_value()
		});
	}
}

/// Only used by Cpp Vertex Shader
void stream_assembler::fetch_vertex(vs_input& rv, size_t vert_index) const
{
	typedef tuple<size_t, input_element_desc const*, size_t> tuple_t;
	for(auto const& reg_ied_extra: reg_ied_extra_ )
	{
		auto reg_index	= reg_ied_extra.reg_id;
		auto desc		= reg_ied_extra.desc;

		void const* pdata = element_address(*desc, vert_index);
		rv.attribute(reg_index) = get_vec4( desc->data_format, reg_ied_extra.default_wcomp, pdata);
	}
}

void const* stream_assembler::element_address( input_element_desc const& elem_desc, size_t vert_index ) const
{
	auto buf_desc = stream_buffer_descs_ + elem_desc.input_slot;
	return buf_desc->buf->raw_data( elem_desc.aligned_byte_offset + buf_desc->stride * vert_index + buf_desc->offset );
}

void const* stream_assembler::element_address( semantic_value const& sv, size_t vert_index ) const{
	return element_address(*( layout_->find_desc(sv) ), vert_index);
}

vector<stream_desc> const& stream_assembler::get_stream_descs(vector<size_t> const& slots)
{
	stream_descs_.resize( slots.size() );

	for(size_t i_slot = 0; i_slot < slots.size(); ++i_slot)
	{
		size_t slot = slots[i_slot];
		stream_desc& str_desc = stream_descs_[i_slot];

		str_desc.buffer = stream_buffer_descs_[slot].buf->raw_data(0);
		str_desc.offset = stream_buffer_descs_[slot].offset;
		str_desc.stride = stream_buffer_descs_[slot].stride;
	}

	return stream_descs_;
}

END_NS_SALVIAR();
