#include "salvia/ext/resource/mesh/mesh_impl.h"
#include "salvia/core/sync_renderer.h"
#include "salvia/resource/resource_manager.h"

#include "eflib/diagnostics/assert.h"

using namespace std;
using namespace eflib;
using namespace salvia::core;

namespace salvia::ext::resource {

mesh_impl::mesh_impl(salvia::core::renderer *psr) {
  assert(psr);

  device_ = psr;
  primcount_ = 0;
  index_format_ = format_unknown;
}

/*
inherited
*/
size_t mesh_impl::get_buffer_count() { return vertex_buffers_.size(); }

size_t mesh_impl::get_face_count() { return primcount_; }

salvia::resource::buffer_ptr mesh_impl::get_index_buffer() { return index_buffer_; }

salvia::resource::buffer_ptr mesh_impl::get_vertex_buffer(size_t buffer_index) {
  if (buffer_index < vertex_buffers_.size()) {
    return vertex_buffers_[buffer_index];
  } else {
    return buffer_ptr();
  }
}

void mesh_impl::gen_adjancency() { ef_unimplemented(); }

void mesh_impl::render() {
  assert(device_);
  if (!device_)
    return;

  if (device_->get_vertex_shader_code()) {
    cached_layout_ = device_->create_input_layout(&(elem_descs_[0]), elem_descs_.size(),
                                                  device_->get_vertex_shader_code());
  } else if (device_->get_vertex_shader()) {
    cached_layout_ = device_->create_input_layout(&(elem_descs_[0]), elem_descs_.size(),
                                                  device_->get_vertex_shader());
  } else {
    return;
  }
  if (!cached_layout_) {
    return;
  }
  for (size_t i_buffer = 0; i_buffer < vertex_buffers_.size(); ++i_buffer) {
    device_->set_vertex_buffers(slots_[i_buffer], 1, &vertex_buffers_[i_buffer],
                                &(strides_[i_buffer]), &(offsets_[i_buffer]));
  }

  device_->set_index_buffer(get_index_buffer(), index_format_);
  device_->set_input_layout(cached_layout_);
  device_->set_primitive_topology(primitive_triangle_list);

  device_->draw_index(0, primcount_, 0);
}

/*
mesh
*/
salvia::resource::buffer_ptr mesh_impl::create_buffer(size_t size) { return device_->create_buffer(size); }

void mesh_impl::set_primitive_count(size_t primcount) { primcount_ = primcount; }

void mesh_impl::set_index_buffer(salvia::resource::buffer_ptr const &v) { index_buffer_ = v; }

void mesh_impl::set_index_type(format fmt) {
  switch (fmt) {
  case format_r16_uint:
  case format_r32_uint:
    index_format_ = fmt;
    break;
  default:
    EFLIB_ASSERT_UNEXPECTED();
  }
}

void mesh_impl::add_vertex_buffer(size_t slot, salvia::resource::buffer_ptr const &buf, size_t stride,
                                  size_t offset) {
  assert(buf);
  vertex_buffers_.push_back(buf);
  strides_.push_back(stride);
  offsets_.push_back(offset);
  slots_.push_back(slot);
}

void mesh_impl::set_input_element_descs(const vector<input_element_desc> &descs) {
  elem_descs_ = descs;
}

salvia::ext::resource::attached_data_ptr mesh_impl::get_attached() { return attached_; }

void mesh_impl::set_attached_data(attached_data_ptr const &attached) { attached_ = attached; }

} // namespace salvia::ext::resource

/*
Copyright (C) 2007-2012 Ye Wu, Minmin Gong

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
