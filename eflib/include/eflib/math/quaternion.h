#pragma once

#include "matrix.h"
#include "vector.h"

namespace eflib {

class euler_angle {
public:
  float yaw, pitch, roll;
};

class quaternion {
private:
  float x, y, z, w;

public:
  // Constructors
  quaternion() = default;
  quaternion(float x, float y, float z, float w);
  explicit quaternion(const vec4& raw_v);

  static quaternion from_axis_angle(const vec3& axis, float angle);
  static quaternion from_mat44(const mat44& mat);

  // Unimplemented but useful functions.
  // static quaternion from_euler(const euler_angle &ea);
  // static quaternion rotate_to(const vec3 &from, const vec3 &to);

  // negative operator
  quaternion operator-() const;

  quaternion& operator*=(const quaternion& rhs);

  [[nodiscard]] float norm() const;

  [[nodiscard]] vec3 axis() const;
  [[nodiscard]] float angle() const;

  [[nodiscard]] vec4 comps() const;

  [[nodiscard]] mat44 to_mat44() const;

  void normalize();

  // friend operators
  friend quaternion normalize(const quaternion& lhs);

  friend quaternion conj(const quaternion& lhs);
  friend quaternion inv(const quaternion& lhs);

  friend quaternion exp(const quaternion& lhs);
  friend quaternion pow(const quaternion& lhs, float t);
  friend quaternion log(const quaternion& lhs);

  friend quaternion operator*(const quaternion& lhs, const quaternion& rhs);
  friend quaternion operator*(const quaternion& q, float scalar);
  friend quaternion operator*(float scalar, const quaternion& q);

  friend quaternion operator/(const quaternion& lhs, const quaternion& rhs);
  friend quaternion operator/(const quaternion& q, float scalar);

  friend vec3& transform(vec3& out, const quaternion& q, const vec3& v);

  friend quaternion lerp(const quaternion& src, const quaternion& dest, float t);
  friend quaternion slerp(const quaternion& src, const quaternion& dest, float t);
};

quaternion normalize(const quaternion& lhs);

quaternion conj(const quaternion& lhs);
quaternion inv(const quaternion& lhs);

quaternion exp(const quaternion& lhs);
quaternion pow(const quaternion& lhs, float t);
quaternion log(const quaternion& lhs);

quaternion operator*(const quaternion& lhs, const quaternion& rhs);
quaternion operator*(const quaternion& q, float scalar);
quaternion operator*(float scalar, const quaternion& q);

quaternion operator/(const quaternion& lhs, const quaternion& rhs);
quaternion operator/(const quaternion& q, float scalar);

vec3& transform(vec3& out, const quaternion& q, const vec3& v);

quaternion lerp(const quaternion& src, const quaternion& dest, float t);
quaternion slerp(const quaternion& src, const quaternion& dest, float t);
}  // namespace eflib
