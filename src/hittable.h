#ifndef HITTABLE_H
#define HITTABLE_H

#include "ray.h"

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    float t;
    bool front_face;
};

class Hittable {
public:
    virtual bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const = 0;
};

#endif
