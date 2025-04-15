#ifndef VEC3_H
#define VEC3_H

#include <cmath>

class Vec3 {
public:
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float t) const { return Vec3(x * t, y * t, z * t); }
    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const { float len = length(); return len > 0 ? *this * (1 / len) : *this; }
};

#endif
