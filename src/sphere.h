#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"
#include "vec3.h"

class Sphere : public Hittable {
public:
    Vec3 center;
    float radius;

    Sphere(const Vec3& c, float r) : center(c), radius(r) {}

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        Vec3 oc = r.origin() - center;
        float a = r.direction().dot(r.direction());
        float b = 2.0f * oc.dot(r.direction());
        float c = oc.dot(oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return false;
        float sqrt_d = std::sqrt(discriminant);
        float t = (-b - sqrt_d) / (2.0f * a);
        if (t < t_min || t > t_max) {
            t = (-b + sqrt_d) / (2.0f * a);
            if (t < t_min || t > t_max) return false;
        }

        rec.t = t;
        rec.point = r.at(t);
        Vec3 outward_normal = (rec.point - center) * (1 / radius);
        rec.front_face = r.direction().dot(outward_normal) < 0;
        rec.normal = rec.front_face ? outward_normal : -outward_normal;
        return true;
    }
};

#endif
