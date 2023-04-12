#include <salvia/resource/buffer.h>

#include <salvia/resource/internal_mapped_resource.h>

#include <cstring>

namespace salvia::resource {

result buffer::map(internal_mapped_resource& mapped, map_mode mm) {
  switch (mm) {
  case map_read:
    mapped.data = mapped.reallocator(data_.size());
    std::memcpy(mapped.data, data_.data(), data_.size());
    break;
  case map_read_write:
  case map_write_no_overwrite:
  case map_write_discard:
  case map_write: mapped.data = data_.data(); break;
  default: ef_unreachable("Unknown map mode."); return result::failed;
  }

  mapped.depth_pitch = mapped.row_pitch = static_cast<uint32_t>(size());

  return result::ok;
}

result buffer::unmap(internal_mapped_resource& /*mapped*/, map_mode /*mm*/) {
  // No intermediate buffer needed in linear mode.
  return result::ok;
}

void buffer::transfer(size_t offset, void const* psrcdata, size_t sz, size_t count) {
  EFLIB_ASSERT_AND_IF(offset + sz * count <= size(), "Out of boundary of buffer.") {
    return;
  }

  uint8_t* dest = raw_data(offset);

  memcpy(dest, psrcdata, sz * count);
}

void buffer::transfer(size_t offset,
                      void const* psrcdata,
                      size_t stride_dest,
                      size_t stride_src,
                      size_t sz,
                      size_t count) {
  EFLIB_ASSERT_AND_IF(offset + stride_dest * (count - 1) + sz <= size(), "Out of buffer.") {
    return;
  }

  auto dest = raw_data(offset);
  auto src = reinterpret_cast<uint8_t const*>(psrcdata);

  if (stride_dest == stride_src && stride_src == sz) {
    // Optimized for continuous memory layout.
    transfer(offset, psrcdata, sz, count);
  } else {
    for (size_t i = 0; i < count; ++i) {
      memcpy(dest, src, sz);
      src += stride_src;
      dest += stride_dest;
    }
  }
}

}  // namespace salvia::resource
