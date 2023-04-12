#include <salvia/core/stream_state.h>

using std::distance;

namespace salvia::core {

void stream_state::update(size_t starts_slot,
                          size_t buffers_count,
                          resource::buffer_ptr const* bufs,
                          size_t const* strides,
                          size_t const* offsets) {
  if (bufs == nullptr || strides == nullptr || offsets == nullptr) {
    return;
  }

  if (starts_slot + buffers_count >= MAX_INPUT_SLOTS) {
    return;
  }

  for (size_t i = 0; i < buffers_count; ++i) {
    buffer_descs[starts_slot + i].buf = bufs[i];
    buffer_descs[starts_slot + i].offset = offsets[i];
    buffer_descs[starts_slot + i].stride = strides[i];
    buffer_descs[starts_slot + i].slot = starts_slot + i;
  }
}

}  // namespace salvia::core
