#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"
#include "utility.h"

class sphere : public hittable {
public:
    vec3 center;
    float radius;

    sphere(const vec3& c, float r) : center(c), radius(r) {}

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - center;
        float a = r.direction().dot(r.direction());
        float b = 2.0f * oc.dot(r.direction());
        float c = oc.dot(oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return false;
        float sqrt_d = std::sqrt(discriminant);
        float t = (-b - sqrt_d) / (2.0f * a);
        if (!ray_t.surrounds(t)) {
            t = (-b + sqrt_d) / (2.0f * a);
            if (!ray_t.surrounds(t))
                return false;
        }

        rec.t = t;
        rec.point = r.at(t);
        vec3 outward_normal = (rec.point - center) * (1 / radius);
        rec.front_face = r.direction().dot(outward_normal) < 0;// Like a V, incidence from outside so the incidence will and normal will be obtus
        rec.normal = rec.front_face ? outward_normal : -outward_normal;
        return true;
    }
};

#endif
