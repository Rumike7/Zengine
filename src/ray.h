#ifndef RAY_H
#define RAY_H

#include "vec3.h"

class Ray {
public:
    Vec3 origin, direction;
    Ray() {}
    Ray(const Vec3& orig, const Vec3& dir) : origin(orig), direction(dir) {}
    Vec3 at(float t) const { return origin + direction * t; }
};

#endif
