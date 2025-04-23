#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"
#include "utility.h"

using point3 = vec3;
class sphere : public hittable {
public:

    // Stationary Sphere
    sphere(const point3& static_center, double radius, shared_ptr<material> mat)
      : center(static_center, vec3(0,0,0)), radius(std::fmax(0,radius)), mat(mat)
    {
        auto rvec = vec3(radius, radius, radius);
        bbox = aabb(static_center - rvec, static_center + rvec);
    }

    aabb bounding_box() const override { return bbox; }
    // Moving Sphere
    sphere(const point3& center1, const point3& center2, double radius,
      shared_ptr<material> mat)
      : center(center1, center2 - center1), radius(std::fmax(0,radius)), mat(mat)
    {
      auto rvec = vec3(radius, radius, radius);
      aabb box1(center.at(0) - rvec, center.at(0) + rvec);
      aabb box2(center.at(1) - rvec, center.at(1) + rvec);
      bbox = aabb(box1, box2);
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        point3 current_center = center.at(r.time());
        vec3 oc = current_center - r.origin();
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
        vec3 outward_normal = (rec.p - current_center) / radius;
        rec.front_face = r.direction().dot(outward_normal) < 0;// Like a V, incidence from outside so the incidence will and normal will be obtus
        rec.normal = rec.front_face ? outward_normal : -outward_normal;
        rec.mat = mat;
        return true;
    }
  private : 
    ray center;
    double radius;
    shared_ptr<material> mat;
    aabb bbox;

};

#endif
