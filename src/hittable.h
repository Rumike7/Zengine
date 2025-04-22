#ifndef HITTABLE_H
#define HITTABLE_H

#include "utility.h"

struct hit_record {
    vec3 point;
    vec3 normal;
    float t;
    bool front_face;
};

class hittable {
public:
    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
};

#endif
