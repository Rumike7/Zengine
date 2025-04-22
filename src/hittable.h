#ifndef HITTABLE_H
#define HITTABLE_H

#include "utility.h"

class material;// Tell the compiler it's a task that we'll be define later, as we only need pointer

struct hit_record {
    vec3 p;
    vec3 normal;
    shared_ptr<material> mat;
    float t;
    bool front_face;
};

class hittable {
public:
    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
};

#endif
