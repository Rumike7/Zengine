#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"
#include "utility.h"

using point3 = vec3;
class sphere : public hittable {
public:
    point3 center;
    double radius;
    shared_ptr<material> mat;

    sphere(const point3& center, double radius, shared_ptr<material> mat)
      : center(center), radius(std::fmax(0,radius)), mat(mat) {}

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
        rec.p = r.at(t);
        vec3 outward_normal = (rec.p - center) * (1 / radius);
        rec.front_face = r.direction().dot(outward_normal) < 0;// Like a V, incidence from outside so the incidence will and normal will be obtus
        rec.normal = rec.front_face ? outward_normal : -outward_normal;
        rec.mat = mat;
        return true;
    }
};

#endif
