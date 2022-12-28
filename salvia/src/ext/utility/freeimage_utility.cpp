#include <salvia/ext/utility/freeimage_utilities.h>

using namespace std;
using namespace eflib;

namespace salvia::ext::utility {

FIBITMAP *load_image(const std::string &filename, int flag) {
  FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(filename.c_str(), 0);
  if (fif == FIF_UNKNOWN) {
    fif = FreeImage_GetFIFFromFilename(filename.c_str());
  }
  if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif)) {
    return FreeImage_Load(fif, filename.c_str(), flag);
  }

  return nullptr;
}

bool check_image_type_support(FIBITMAP *image) {
  if (nullptr == image) {
    return false;
  }

  FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(image);

  switch (image_type) {
  case FIT_UNKNOWN:
  case FIT_UINT16:
  case FIT_INT16:
  case FIT_UINT32:
  case FIT_INT32:
  case FIT_FLOAT:
  case FIT_DOUBLE:
  case FIT_COMPLEX:
  case FIT_RGB16:
  case FIT_RGBA16:
  case FIT_RGBF:
    return false;
  case FIT_RGBAF:
    return true;
  case FIT_BITMAP:
    if (FreeImage_GetColorType(image) == FIC_RGB) {
      size_t bpp = FreeImage_GetBPP(image);
      if (bpp == 24 || bpp == 32)
        return true;
      return false;
    }
    if (FreeImage_GetColorType(image) == FIC_RGBALPHA) {
      return true;
    }
  }
  return false;
}

FIBITMAP *make_bitmap_copy(rect<size_t> &out_region, size_t dest_width, size_t dest_height,
                           FIBITMAP *image, const rect<size_t> &src_region) {
  if (image == nullptr || !check_image_type_support(image)) {
    return nullptr;
  }

  size_t img_w = FreeImage_GetWidth(image);
  size_t img_h = FreeImage_GetHeight(image);

  // If the dest is same size with source, just return the source image and region.
  if (dest_width == src_region.w && dest_height == src_region.h) {
    out_region = src_region;
    return image;
  }

  // If only part of image is copied, region should be copied to a new image.
  FIBITMAP *sub_image = nullptr;
  if (src_region.x == 0 && src_region.y == 0 && src_region.w == img_w && src_region.h == img_h) {
    sub_image = image;
  } else {
    sub_image =
        FreeImage_Copy(image, (int)src_region.x, (int)src_region.y,
                       (int)(src_region.x + src_region.w), int(src_region.y + src_region.h));
    FreeImage_Unload(image);
    if (!sub_image)
      return nullptr;
  }

  // scale
  FIBITMAP *scaled_image = nullptr;
  scaled_image = FreeImage_Rescale(sub_image, int(dest_width), int(dest_height), FILTER_BILINEAR);
  FreeImage_Unload(sub_image);

  out_region = rect<size_t>(0, 0, dest_width, dest_height);

  return scaled_image;
}

}
