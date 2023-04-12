#pragma once

#include <sasl/semantic/semantic_forward.h>

#include <eflib/platform/stdint.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>

namespace sasl::semantic {

struct elem_indexes {
  explicit elem_indexes(char index0 = -1, char index1 = -1, char index2 = -1, char index3 = -1) {
    data[0] = index0;
    data[1] = index1;
    data[2] = index2;
    data[3] = index3;
  }

  explicit elem_indexes(std::string_view index_str) {
    data[0] = -1;
    data[1] = -1;
    data[2] = -1;
    data[3] = -1;

    int i = 0;
    for (auto ch : index_str) {
      char ch_index = -1;
      switch (ch) {
      case 'x':
      case 'r': ch_index = 0; break;
      case 'y':
      case 'g': ch_index = 1; break;
      case 'z':
      case 'b': ch_index = 2; break;
      case 'w':
      case 'a': ch_index = 3; break;
      }
      data[i] = ch_index;
      ++i;
    }
  }

  static elem_indexes from_length(size_t len) {
    elem_indexes ret;
    assert(0 <= len && len <= 4);
    for (size_t i = 0; i < len; ++i) {
      ret.data[i] = static_cast<char>(i);
    }
    return ret;
  }

  char data[4];

  char operator[](size_t index) const { return data[index]; }

  bool empty() const { return data[0] == -1; }

  char max_element() const { return *(std::max_element(data, data + 4)); }

  size_t length() const {
    for (size_t i = 0; i < 4; ++i) {
      if (data[i] == -1)
        return i;
    }
    return 4;
  }
};

}  // namespace sasl::semantic
