#pragma once

#include <eflib/platform/stdint.h>
#include <eflib/utility/enum.h>
#include <functional>

enum class type_qualifiers : uint32_t { _uniform = UINT32_C(2), none = UINT32_C(1) };

void register_enum_name(std::function<void(char const*, type_qualifiers)> const& reg_fn);
