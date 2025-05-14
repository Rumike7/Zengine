#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"

using point3 = vec3;

class sphere : public hittable {
  public:
    // Stationary Sphere
    sphere(const point3& static_center, double radius, shared_ptr<material> mat,int id)
      : center(static_center, vec3(0,0,0)), radius(std::fmax(0,radius)), mat(mat)
    {
      this->id = id;
      set_bounding_box();

    }

    // Moving Sphere // Motion blur
    sphere(const point3& center1, const point3& center2, double radius, shared_ptr<material> mat, int id)
      : center(center1, center2 - center1), radius(std::fmax(0,radius)), mat(mat)
    {
      this->id = id;
      set_bounding_box();
    }



    void set_bounding_box() override {
        auto rvec = vec3(radius, radius, radius);
        aabb box1(center.at(0) - rvec, center.at(0) + rvec);
        aabb box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = aabb(box1, box2);
    }

    void move_by(const point3& offset) override {
        // Preserve motion blur by translating center1 and center2
        vec3 velocity = center.direction(); // center2 - center1
        point3 center1 = center.origin();
        point3 center2 = center1 + velocity;
        center = ray(center1 + offset, velocity); // Update center1, keep velocity
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        point3 current_center = center.at(r.time());
        vec3 oc = current_center - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - radius*radius;

        auto discriminant = h*h - a*c;
        if (discriminant < 0)
            return false;

        auto sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        auto root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root))
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat;

        return true;
    }

    std::ostream& print(std::ostream& out) const override{
        out << "Sphere("
            << "center=" << center
            << ", radius=" << radius << ")";
        return out;
    }
    std::istream& write(std::istream& in) const override{
        return in;
    }



  private:
    ray center;
    double radius;
    shared_ptr<material> mat;

    // y, x, z =−cos(θ), −cos(ϕ)sin(θ), sin(ϕ)sin(θ)
    static void get_sphere_uv(const point3& p, double& u, double& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = std::acos(-p.y());
        auto phi = std::atan2(-p.z(), p.x()) + pi;

        u = phi / (2*pi);
        v = theta / pi;
    }
};
#endif

//  std::shared_ptr<sphere> sphere0 = std::make_shared<sphere>(*static_cast<sphere*>(obj.get()));            std::clog << (*sphere0) << "\n";
