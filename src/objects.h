#ifndef ADDITIONAL_OBJECTS_H
#define ADDITIONAL_OBJECTS_H

#include "hittable.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include "quad.h"
#include "sphere.h"
#include "solve.h"

class cylinder : public hittable {
public:
    cylinder(const point3& base, const vec3& axis, double radius, double height)
        : base(base), axis(unit_vector(axis)), radius(std::fmax(0, radius)), height(std::fmax(0, height)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - base;
        vec3 dir = r.direction();
        vec3 n = axis;

        bool hit_anything = false;
        hit_record temp_rec;

        // --- 1. Lateral surface (side of cylinder) ---
        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n));
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - radius * radius;
        auto discriminant = b * b - 4 * a * c;

        if (discriminant >= 0) {
            auto sqrtd = std::sqrt(discriminant);
            for (int i = 0; i < 2; ++i) {
                auto t = (-b + (i == 0 ? -sqrtd : sqrtd)) / (2.0 * a);
                if (!ray_t.contains(t)) continue;

                auto p = r.at(t);
                auto h = dot(p - base, n);
                if (h >= 0 && h <= height) {
                    temp_rec.t = t;
                    temp_rec.p = p;
                    vec3 outward_normal = (p - (base + h * n)) / radius;
                    temp_rec.set_face_normal(r, outward_normal);
                    temp_rec.u = std::atan2(dot(p - base, cross(n, vec3(1,0,0))), dot(p - base, cross(n, vec3(0,1,0)))) / (2 * pi);
                    temp_rec.v = h / height;
                    temp_rec.mat = mat;
                    ray_t.max = t;
                    rec = temp_rec;
                    hit_anything = true;
                }
            }
        }

        // --- 2. Bottom cap ---
        {
            auto denom = dot(r.direction(), n);
            if (std::fabs(denom) > 1e-8) {
                auto t = dot(base - r.origin(), n) / denom;
                if (ray_t.contains(t)) {
                    point3 p = r.at(t);
                    if (glm::length2((p - base)) <= radius * radius) {
                        temp_rec.t = t;
                        temp_rec.p = p;
                        temp_rec.set_face_normal(r, -n);
                        temp_rec.u = 0.5 + 0.5 * std::atan2(p.x, p.z) / pi;
                        temp_rec.v = 0.5 - 0.5 * p.y / radius;
                        temp_rec.mat = mat;
                        ray_t.max = t;
                        rec = temp_rec;
                        hit_anything = true;
                    }
                }
            }
        }

        // --- 3. Top cap ---
        {
            point3 top_center = base + height * n;
            auto denom = dot(r.direction(), n);
            if (std::fabs(denom) > 1e-8) {
                auto t = dot(top_center - r.origin(), n) / denom;
                if (ray_t.contains(t)) {
                    point3 p = r.at(t);
                    if (glm::length2((p - top_center)) <= radius * radius) {
                        temp_rec.t = t;
                        temp_rec.p = p;
                        temp_rec.set_face_normal(r, n);
                        temp_rec.u = 0.5 + 0.5 * std::atan2(p.x, p.z) / pi;
                        temp_rec.v = 0.5 + 0.5 * p.y / radius;
                        temp_rec.mat = mat;
                        ray_t.max = t;
                        rec = temp_rec;
                        hit_anything = true;
                    }
                }
            }
        }

        return hit_anything;
    }


    void set_bounding_box() override {
        auto rvec = vec3(radius, radius, radius);
        auto p1 = base;
        auto p2 = base + height * axis;
        bbox = aabb(p1 - rvec, p2 + rvec);
    }

    void move_by(const point3& offset) override {
        base = base + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Cylinder(base=" << base << ", axis=" << axis << ", radius=" << radius << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    double radius, height;
};

class cone : public hittable {
public:
    cone(const point3& base, const vec3& axis, double radius, double height)
        : base(base), axis(unit_vector(axis)), radius(std::fmax(0, radius)), height(std::fmax(0, height)) {
        
        this->axis = -this->axis;
        
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - base;
        vec3 dir = r.direction();
        vec3 n = axis;

        double k = radius / height;
        double d_dot_n = dot(dir, n);
        double oc_dot_n = dot(oc, n);

        // Quadratic equation coefficients for cone surface
        double a = dot(dir, dir) - d_dot_n * d_dot_n * (1 + k * k);
        double b = 2.0 * (dot(oc, dir) - oc_dot_n * d_dot_n * (1 + k * k));
        double c = dot(oc, oc) - oc_dot_n * oc_dot_n * (1 + k * k);

        if (std::fabs(a) < 1e-8) {
            return false;
        }

        double discriminant = b * b - 4 * a * c;
        if (discriminant < 0) {
            return false;
        }

        double sqrtd = std::sqrt(discriminant);
        double t = (-b - sqrtd) / (2.0 * a);
        if (!ray_t.contains(t)) {
            t = (-b + sqrtd) / (2.0 * a);
            if (!ray_t.contains(t)) {
                return false;
            }
        }

        // Check if the intersection point is within the cone's height
        point3 p = r.at(t);
        double h = dot(p - base, n);
        if (h < 0 || h > height) {
            return false;
        }

        // Valid intersection on the cone surface
        rec.t = t;
        rec.p = p;

        vec3 base_to_point = p - base;
        vec3 axial_component = dot(base_to_point, n) * n;
        vec3 radial_component = base_to_point - axial_component;

        double taper_factor = 1 - h / height;
        if (std::fabs(taper_factor) < 1e-8) taper_factor = 1e-8;

        vec3 outward_normal = unit_vector((radial_component - k * h * n));
        rec.set_face_normal(r, outward_normal);
        rec.mat = mat;

        // UV calculation
        vec3 local_x = unit_vector(cross(n, vec3(1, 0, 0)));
        if (glm::length2(local_x) < 1e-6) local_x = unit_vector(cross(n, vec3(0, 1, 0)));
        vec3 local_y = cross(n, local_x);
        vec3 rel = base_to_point - h * n;
        double u_denominator = dot(rel, local_x);
        if (std::fabs(u_denominator) < 1e-8) u_denominator = 1e-8;
        rec.u = (std::atan2(dot(rel, local_y), u_denominator) + 2 * M_PI) / (2 * M_PI);
        rec.v = std::clamp(h / height, 0.0, 1.0);

        rec.mat = mat;
        return true;
    }
    
    void set_bounding_box() override {
        point3 apex = base + height * axis;
        double apex_radius = 0.0; // Apex has no radius (point)
        // Define the bounding box to enclose the cone from base to apex
        vec3 rvec = vec3(radius, radius, radius); // Radius vector for base
        point3 min_point = glm::min(base - rvec, apex);
        point3 max_point = glm::max(base + rvec, apex);
        bbox = aabb(min_point, max_point);
    }

    void set_material(std::shared_ptr<material> m) override {
        mat = m;
    }

    void move_by(const point3& offset) override {
        base += offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Cone(base=" << base << ", axis=" << axis << ", radius=" << radius << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    double radius, height;
};

class torus : public hittable {
public:
    torus(const point3& center, double major_radius, double minor_radius)
        : center(center), major_radius(std::fmax(0, major_radius)), minor_radius(std::fmax(0, minor_radius)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 O = r.origin() - center;
        vec3 D = r.direction();

        double R = major_radius;
        double r0 = minor_radius;

        double dx = D.x, dy = D.y, dz = D.z;
        double ox = O.x, oy = O.y, oz = O.z;

        double G = dx * dx + dy * dy + dz * dz;
        double H = 2.0 * (dx * ox + dy * oy + dz * oz);
        double I = ox * ox + oy * oy + oz * oz + R * R - r0 * r0;

        double K = dx * dx + dy * dy + dz * dz;
        double J = ox * ox + oy * oy + oz * oz - R * R - r0 * r0;

        // Intermediate terms
        double L = 2.0 * (dx * ox + dy * oy + dz * oz);
        double M = ox * ox + oy * oy + oz * oz + R * R - r0 * r0;

        // Compute quartic coefficients
        double sum_d_sq = glm::length2(D);
        double e = dot(O, O) + R * R - r0 * r0;
        double f = dot(O, D);
        double four_R2 = 4.0 * R * R;

        double A = sum_d_sq * sum_d_sq;
        double B = 4.0 * sum_d_sq * f;
        double C = 2.0 * sum_d_sq * e + 4.0 * f * f - four_R2 * (dy * dy);
        double Dq = 4.0 * f * e - 2.0 * four_R2 * oy * dy;
        double E = e * e - four_R2 * (oy * oy - r0 * r0);

        // Solve quartic
        double roots[4];
        int num_roots = solve_quartic(A, B, C, Dq, E, roots);
        double t = -1;
        for (int i = 0; i < num_roots; ++i) {
            if (ray_t.contains(roots[i]) && (t < 0 || roots[i] < t)) {
                t = roots[i];
            }
        }
        if (t < 0) return false;

        rec.t = t;
        rec.p = r.at(t);
        vec3 normal = compute_torus_normal(rec.p);
        rec.set_face_normal(r, normal);

        // Parameterization (u, v) based on toroidal/poloidal angles
        vec3 p_rel = rec.p - center;
        double phi = atan2(p_rel.z, p_rel.x);
        double len = sqrt(p_rel.x * p_rel.x + p_rel.z * p_rel.z);
        double theta = atan2(p_rel.y, len - R);
        rec.u = phi / (2 * pi);
        rec.v = theta / (2 * pi);

        rec.mat = mat;
        return true;
    }



    void set_bounding_box() override {
        auto rvec = vec3(major_radius + minor_radius, minor_radius, major_radius + minor_radius);
        bbox = aabb(center - rvec, center + rvec);
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Torus(center=" << center << ", major_radius=" << major_radius << ", minor_radius=" << minor_radius << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    double major_radius, minor_radius;

    vec3 compute_torus_normal(const point3& p) const {
        double x = p.x - center.x;
        double y = p.y - center.y;
        double z = p.z - center.z;
        double R = major_radius;
        double r = minor_radius;
        double s = sqrt(x * x + z * z);
        return unit_vector(vec3(x * (1 - R / s), y, z * (1 - R / s)));
    }

};

class plane : public hittable {
public:
    plane(const point3& point, const vec3& normal)
        : point(point), normal(unit_vector(normal)) {
        D = dot(normal, point);
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction());
        if (std::fabs(denom) < 1e-8) return false;

        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;

        rec.t = t;
        rec.p = r.at(t);
        rec.set_face_normal(r, normal);
        rec.u = 0; // No UV for infinite plane
        rec.v = 0;
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        bbox = aabb(); // Infinite plane has no bounds
    }

    void move_by(const point3& offset) override {
        point = point + offset;
        D = dot(normal, point);
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Plane(point=" << point << ", normal=" << normal << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 point;
    vec3 normal;
    double D;
};

class ellipsoid : public hittable {
public:
    ellipsoid(const point3& center, const vec3& a, const vec3& b, const vec3& c)
        : center(center), a(a), b(b), c(c) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        mat3 M(a, b, c); // Matrix with axes as columns
        mat3 M_inv = M.inverse();
        vec3 oc = M_inv * (r.origin() - center);
        vec3 dir = M_inv * r.direction();

        auto aa = glm::length2(dir);
        auto bb = 2.0 * dot(oc, dir);
        auto cc = glm::length2(oc) - 1.0;

        auto discriminant = bb * bb - 4 * aa * cc;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        auto t = (-bb - sqrtd) / (2.0 * aa);
        if (!ray_t.contains(t)) {
            t = (-bb + sqrtd) / (2.0 * aa);
            if (!ray_t.contains(t)) return false;
        }

        rec.t = t;
        rec.p = r.at(t);
        vec3 normal = M_inv.transpose() * (M_inv * (rec.p - center));
        rec.set_face_normal(r, unit_vector(normal));
        rec.u = std::atan2(rec.p.z, rec.p.x) / (2 * pi);
        rec.v = std::acos(rec.p.y / a.length()) / pi;
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        auto rvec = vec3(a.length(), b.length(), c.length());
        bbox = aabb(center - rvec, center + rvec);
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Ellipsoid(center=" << center << ", a=" << a << ", b=" << b << ", c=" << c << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    vec3 a, b, c;
};

class capsule : public hittable {
public:
    capsule(const point3& p1, const point3& p2, double radius)
        : p1(p1), p2(p2), radius(std::fmax(0, radius)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 axis = p2 - p1;
        double len = axis.length();
        vec3 n = axis / len;
        vec3 oc = r.origin() - p1;
        vec3 dir = r.direction();

        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n));
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - radius * radius;

        auto discriminant = b * b - 4 * a * c;
        bool hit_cylinder = false;
        double t = -1;
        if (discriminant >= 0) {
            auto sqrtd = std::sqrt(discriminant);
            t = (-b - sqrtd) / (2.0 * a);
            if (!ray_t.contains(t)) {
                t = (-b + sqrtd) / (2.0 * a);
            }
            if (ray_t.contains(t)) {
                auto p = r.at(t);
                auto h = dot(p - p1, n);
                if (h >= 0 && h <= len) {
                    hit_cylinder = true;
                    rec.t = t;
                    rec.p = p;
                    rec.set_face_normal(r, (p - (p1 + h * n)) / radius);
                }
            }
        }

        // Check spherical caps
        sphere cap1(p1, radius);
        sphere cap2(p2, radius);
        hit_record temp_rec;
        bool hit_cap = false;
        if (cap1.hit(r, ray_t, temp_rec) && (!hit_cylinder || temp_rec.t < t)) {
            rec = temp_rec;
            hit_cap = true;
        }
        if (cap2.hit(r, ray_t, temp_rec) && (!hit_cylinder || temp_rec.t < t) && (!hit_cap || temp_rec.t < rec.t)) {
            rec = temp_rec;
            hit_cap = true;
        }

        if (!hit_cylinder && !hit_cap) return false;
        rec.u = 0; // Simplified UV
        rec.v = 0;
        rec.mat = mat;
        return true;
    }

    void set_bounding_box() override {
        auto rvec = vec3(radius, radius, radius);
        bbox = aabb(p1 - rvec, p2 + rvec);
    }

    void move_by(const point3& offset) override {
        p1 = p1 + offset;
        p2 = p2 + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Capsule(p1=" << p1 << ", p2=" << p2 << ", radius=" << radius << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 p1, p2;
    double radius;
};

class hollow_cylinder : public hittable {
public:
    hollow_cylinder(const point3& base, const vec3& axis, double outer_radius, double inner_radius, double height)
        : base(base),
          axis(unit_vector(axis)),
          outer_radius(std::fmax(inner_radius, outer_radius)),
          inner_radius(std::fmax(0, std::fmin(inner_radius, outer_radius))),
          height(std::fmax(0, height)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        bool hit_anything = false;
        hit_record temp_rec;

        // Check outer lateral surface
        if (hit_lateral_surface(r, ray_t, outer_radius, axis, base, height, true, temp_rec)) {
            ray_t.max = temp_rec.t;
            rec = temp_rec;
            hit_anything = true;
        }

        // Check inner lateral surface
        if (inner_radius > 0 && hit_lateral_surface(r, ray_t, inner_radius, axis, base, height, false, temp_rec)) {
            ray_t.max = temp_rec.t;
            rec = temp_rec;
            hit_anything = true;
        }

        // Check bottom ring (cap with hole)
        if (hit_ring_cap(r, ray_t, base, -axis, temp_rec)) {
            ray_t.max = temp_rec.t;
            rec = temp_rec;
            hit_anything = true;
        }

        // Check top ring (cap with hole)
        point3 top_center = base + height * axis;
        if (hit_ring_cap(r, ray_t, top_center, axis, temp_rec)) {
            ray_t.max = temp_rec.t;
            rec = temp_rec;
            hit_anything = true;
        }

        return hit_anything;
    }

    void set_bounding_box() override {
        auto rvec = vec3(outer_radius, outer_radius, outer_radius);
        auto p1 = base;
        auto p2 = base + height * axis;
        bbox = aabb(p1 - rvec, p2 + rvec);
    }

    void move_by(const point3& offset) override {
        base = base + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "HollowCylinder(base=" << base
            << ", axis=" << axis
            << ", outer_radius=" << outer_radius
            << ", inner_radius=" << inner_radius
            << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    double outer_radius, inner_radius, height;

    // Helper: lateral surface (direction: true = outward, false = inward)
    bool hit_lateral_surface(const ray& r, interval ray_t, double radius, const vec3& n, const point3& base,
                             double height, bool outward, hit_record& rec) const {
        vec3 oc = r.origin() - base;
        vec3 dir = r.direction();

        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n));
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - radius * radius;
        auto discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        for (int i = 0; i < 2; ++i) {
            auto t = (-b + (i == 0 ? -sqrtd : sqrtd)) / (2.0 * a);
            if (!ray_t.contains(t)) continue;

            auto p = r.at(t);
            auto h = dot(p - base, n);
            if (h >= 0 && h <= height) {
                rec.t = t;
                rec.p = p;
                vec3 normal = (p - (base + h * n)) / radius;
                if (!outward) normal = -normal;
                rec.set_face_normal(r, normal);
                rec.u = std::atan2(normal.z, normal.x) / (2 * pi);
                rec.v = h / height;
                rec.mat = mat;
                return true;
            }
        }

        return false;
    }

    // Helper: hit ring (cap with hole)
    bool hit_ring_cap(const ray& r, interval ray_t, const point3& center, const vec3& normal, hit_record& rec) const {
        auto denom = dot(r.direction(), normal);
        if (std::fabs(denom) < 1e-8) return false;

        auto t = dot(center - r.origin(), normal) / denom;
        if (!ray_t.contains(t)) return false;

        point3 p = r.at(t);
        double dist2 = glm::length2((p - center));
        if (dist2 > outer_radius * outer_radius || dist2 < inner_radius * inner_radius) return false;

        rec.t = t;
        rec.p = p;
        rec.set_face_normal(r, normal);
        rec.u = 0.5 + 0.5 * std::atan2(p.z, p.x) / pi;
        rec.v = 0.5 - 0.5 * p.y / outer_radius;
        rec.mat = mat;
        return true;
    }
};


class hexagon : public hittable {
public:
    hexagon(const point3& center, const vec3 n, double radius)
        : center(center), n(n), radius(radius) {        

        vec3 u, v;
        if (fabs(n.x) > 0.9) { 
            u = vec3(0, 1, 0);
        } else {
            u = vec3(1, 0, 0);
        }
        u = unit_vector(cross(u, n));
        v = cross(n, u);
        
        point3 vertices[6];
        for (int i = 0; i < 6; i++) {
            double angle = i * M_PI / 3.0; 
            double x = radius * cos(angle);
            double y = radius * sin(angle);
            vertices[i] = center + (u * x + v * y);
        }   
        
        for (int i = 0; i < 6; i++) {
            int next = (i + 1) % 6;
            
            vec3 edge1 = vertices[i] - center;
            vec3 edge2 = vertices[next] - center;
            
            triangles[i] = std::make_shared<triangle>(center, edge1, edge2);
        }
        
        set_bounding_box();
    }
    
    void set_bounding_box() override {
        bbox = triangles[0]->bounding_box();   
        for (int i = 1; i < 6; i++) {
            bbox = aabb(bbox, triangles[i]->bounding_box());
        }
    
    }

    void set_material(shared_ptr<material> mat){
        this->mat = mat;
        for (int i = 0; i < 6; i++) {
            triangles[i]->set_material(mat);
        }
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        
        for (int i = 0; i < 6; i++) {
            triangles[i]->move_by(offset);
        }
        
        set_bounding_box();
    }
    
    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything = false;
        double closest_so_far = ray_t.max;
        
        // Test ray against all 6 triangles
        for (int i = 0; i < 6; i++) {
            if (triangles[i]->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;

            }
        }
        
        return hit_anything;
    }
    
    std::ostream& print(std::ostream& out) const override {
        out << "Hexagon("
            << "center=" << center
            << ", n=" << n
            << ", radius=" << radius << ")";
        return out;
    }
    
    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    vec3 n;
    double radius;
    std::shared_ptr<triangle> triangles[6];
};


class prism : public hittable_list {
public:
    prism(const point3& base, const vec3& axis, const std::vector<point3>& base_vertices, double height)
        : base(base), axis(unit_vector(axis)), base_vertices(base_vertices), height(height) {
        std::vector<point3> top_vertices;
        for (const auto& v : base_vertices) {
            top_vertices.push_back(v + height * axis);
        }

        // Bottom and top faces
        vec3 u = base_vertices[1] - base_vertices[0];
        vec3 v = base_vertices[2] - base_vertices[0];
        objects.push_back(std::make_shared<triangle>(base_vertices[0], u, v));
        u = top_vertices[1] - top_vertices[0];
        v = top_vertices[2] - top_vertices[0];
        objects.push_back(std::make_shared<triangle>(top_vertices[0], u, v));
        // Side faces
        for (size_t i = 0; i < base_vertices.size(); ++i) {
            auto next = (i + 1) % base_vertices.size();
            objects.push_back(std::make_shared<quad>(base_vertices[i], base_vertices[next] - base_vertices[i], height * axis));
        }
        set_bounding_box();
    }

    void set_bounding_box() override {
        point3 min = base;
        point3 max = base;
        for (const auto& v : base_vertices) {
            min = point3(std::fmin(min.x, v.x), std::fmin(min.y, v.y), std::fmin(min.z, v.z));
            max = point3(std::fmax(max.x, v.x), std::fmax(max.y, v.y), std::fmax(max.z, v.z));
        }
        max = max + height * axis;
        bbox = aabb(min, max);
    }

    void move_by(const point3& offset) override {
        base = base + offset;
        for (size_t i = 0; i < base_vertices.size(); ++i) {
            base_vertices[i] = base_vertices[i] + offset;
        }
        for (auto& obj : objects) {
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Prism(base=" << base << ", axis=" << axis << ", vertices=" << base_vertices.size() << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    std::vector<point3> base_vertices;
    double height;
};

class polyhedron : public hittable_list {
public:
    polyhedron(const std::vector<point3>& vertices, const std::vector<std::vector<int>>& faces)
        : vertices(vertices), faces(faces) {
        for (const auto& face : faces) {
            objects.push_back(std::make_shared<triangle>(vertices[face[0]], vertices[face[1]] - vertices[face[0]], vertices[face[2]] - vertices[face[0]]));
        }
        set_bounding_box();
    }

    void set_bounding_box() override {
        point3 min = vertices[0];
        point3 max = vertices[0];
        for (const auto& v : vertices) {
            min = point3(std::fmin(min.x, v.x), std::fmin(min.y, v.y), std::fmin(min.z, v.z));
            max = point3(std::fmax(max.x, v.x), std::fmax(max.y, v.y), std::fmax(max.z, v.z));
        }
        bbox = aabb(min, max);
    }

    void move_by(const point3& offset) override {
        for (auto& v : vertices) {
            v = v + offset;
        }
        for (auto& obj : objects) {
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Polyhedron(vertices=" << vertices.size() << ", faces=" << faces.size() << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    std::vector<point3> vertices;
    std::vector<std::vector<int>> faces;
};

class frustum : public hittable {
public:
    frustum(const point3& base, const vec3& axis, double base_radius, double top_radius, double height)
        : base(base), axis(unit_vector(axis)), base_radius(std::fmax(0, base_radius)),
          top_radius(std::fmax(0, top_radius)), height(std::fmax(0, height)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - base;
        vec3 dir = r.direction();
        vec3 n = axis;

        double k = (base_radius - top_radius) / height;
        double R = base_radius;
        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n) - k * (dot(oc, n) * dot(dir, n) + dot(dir, n) * dot(oc, n)));
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - R * R + 2 * k * R * dot(oc, n) - k * k * dot(oc, n) * dot(oc, n);

        auto discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        auto t = (-b - sqrtd) / (2.0 * a);
        if (!ray_t.contains(t)) {
            t = (-b + sqrtd) / (2.0 * a);
            if (!ray_t.contains(t)) return false;
        }

        auto p = r.at(t);
        auto h = dot(p - base, n);
        if (h < 0 || h > height) return false;

        rec.t = t;
        rec.p = p;
        vec3 outward_normal = (p - (base + h * n)) / (base_radius - h * k);
        rec.set_face_normal(r, outward_normal);
        rec.u = std::atan2(dot(p - base, cross(n, vec3(1,0,0))), dot(p - base, cross(n, vec3(0,1,0)))) / (2 * pi);
        rec.v = h / height;
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        auto rvec = vec3(std::fmax(base_radius, top_radius), std::fmax(base_radius, top_radius), std::fmax(base_radius, top_radius));
        auto p1 = base;
        auto p2 = base + height * axis;
        bbox = aabb(p1 - rvec, p2 + rvec);
    }

    void move_by(const point3& offset) override {
        base = base + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Frustum(base=" << base << ", axis=" << axis << ", base_radius=" << base_radius
            << ", top_radius=" << top_radius << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    double base_radius, top_radius, height;
};

class wedge : public hittable_list {
public:
    wedge(const point3& p1, const point3& p2, const point3& p3, double height)
        : p1(p1), p2(p2), p3(p3), height(height), axis(vec3(0,1,0)) {
        point3 top1 = p1 + height * axis;
        point3 top2 = p2 + height * axis;
        point3 top3 = p3 + height * axis;
        objects.push_back(std::make_shared<triangle>(p1, p2 - p1, p3 - p1)); // Bottom
        objects.push_back(std::make_shared<triangle>(top1, top2 - top1, top3 - top1)); // Top
        objects.push_back(std::make_shared<quad>(p1, p2 - p1, height * axis)); // Side 1
        objects.push_back(std::make_shared<quad>(p2, p3 - p2, height * axis)); // Side 2
        objects.push_back(std::make_shared<quad>(p3, p1 - p3, height * axis)); // Side 3
        set_bounding_box();
    }

    void set_bounding_box() override {
        point3 min(std::fmin(p1.x, std::fmin(p2.x, p3.x)), std::fmin(p1.y, std::fmin(p2.y, p3.y)), std::fmin(p1.z, std::fmin(p2.z, p3.z)));
        point3 max(std::fmax(p1.x, std::fmax(p2.x, p3.x)), std::fmax(p1.y, std::fmax(p2.y, p3.y)) + height, std::fmax(p1.z, std::fmax(p2.z, p3.z)));
        bbox = aabb(min, max);
    }

    void move_by(const point3& offset) override {
        p1 = p1 + offset;
        p2 = p2 + offset;
        p3 = p3 + offset;
        for (auto& obj : objects) {
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Wedge(p1=" << p1 << ", p2=" << p2 << ", p3=" << p3 << ", height=" << height << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 p1, p2, p3;
    double height;
    vec3 axis;
};

class tetrahedron : public hittable_list {
public:
    tetrahedron(const point3& p1, const point3& p2, const point3& p3, const point3& p4)
        : p1(p1), p2(p2), p3(p3), p4(p4) {
        objects.push_back(std::make_shared<triangle>(p1, p2 - p1, p3 - p1));
        objects.push_back(std::make_shared<triangle>(p1, p3 - p1, p4 - p1));
        objects.push_back(std::make_shared<triangle>(p2, p3 - p2, p4 - p2));
        objects.push_back(std::make_shared<triangle>(p3, p1 - p3, p4 - p3));
        set_bounding_box();
    }

    void set_bounding_box() override {
        point3 min(std::fmin(p1.x, std::fmin(p2.x, std::fmin(p3.x, p4.x))),
                   std::fmin(p1.y, std::fmin(p2.y, std::fmin(p3.y, p4.y))),
                   std::fmin(p1.z, std::fmin(p2.z, std::fmin(p3.z, p4.z))));
        point3 max(std::fmax(p1.x, std::fmax(p2.x, std::fmax(p3.x, p4.x))),
                   std::fmax(p1.y, std::fmax(p2.y, std::fmax(p3.y, p4.y))),
                   std::fmax(p1.z, std::fmax(p2.z, std::fmax(p3.z, p4.z))));
        bbox = aabb(min, max);
    }

    void move_by(const point3& offset) override {
        p1 = p1 + offset;
        p2 = p2 + offset;
        p3 = p3 + offset;
        p4 = p4 + offset;
        for (auto& obj : objects) {
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Tetrahedron(p1=" << p1 << ", p2=" << p2 << ", p3=" << p3 << ", p4=" << p4 << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 p1, p2, p3, p4;
};

class octahedron : public hittable_list {
public:
    octahedron(const point3& center, double size)
        : center(center), size(size) {
        point3 p1(center + vec3(size, 0, 0));
        point3 p2(center - vec3(size, 0, 0));
        point3 p3(center + vec3(0, size, 0));
        point3 p4(center - vec3(0, size, 0));
        point3 p5(center + vec3(0, 0, size));
        point3 p6(center - vec3(0, 0, size));
        objects.push_back(std::make_shared<triangle>(p1, p3, p5));
        objects.push_back(std::make_shared<triangle>(p1, p5, p4));
        objects.push_back(std::make_shared<triangle>(p1, p4, p6));
        objects.push_back(std::make_shared<triangle>(p1, p6, p3));
        objects.push_back(std::make_shared<triangle>(p2, p3, p5));
        objects.push_back(std::make_shared<triangle>(p2, p5, p4));
        objects.push_back(std::make_shared<triangle>(p2, p4, p6));
        objects.push_back(std::make_shared<triangle>(p2, p6, p3));
        set_bounding_box();
    }

    void set_bounding_box() override {
        auto rvec = vec3(size, size, size);
        bbox = aabb(center - rvec, center + rvec);
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        for (auto& obj : objects) {
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Octahedron(center=" << center << ", size=" << size << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    double size;
};

class spherical_shell : public hittable {
public:
    spherical_shell(const point3& center, double inner_radius, double outer_radius)
        : center(center), inner_radius(std::fmax(0, inner_radius)), outer_radius(std::fmax(inner_radius, outer_radius)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - center;
        auto a = glm::length2(r.direction());
        auto h = dot(r.direction(), oc);
        auto c_outer = glm::length2(oc) - outer_radius * outer_radius;
        auto c_inner = glm::length2(oc) - inner_radius * inner_radius;

        auto discriminant_outer = h * h - a * c_outer;
        auto discriminant_inner = h * h - a * c_inner;
        double t = -1;
        vec3 normal;
        bool hit_outer = false;

        if (discriminant_outer >= 0) {
            auto sqrtd = std::sqrt(discriminant_outer);
            t = (h - sqrtd) / a;
            if (!ray_t.contains(t)) {
                t = (h + sqrtd) / a;
            }
            if (ray_t.contains(t)) {
                rec.t = t;
                rec.p = r.at(t);
                normal = (rec.p - center) / outer_radius;
                hit_outer = true;
            }
        }

        if (discriminant_inner >= 0) {
            auto sqrtd = std::sqrt(discriminant_inner);
            double t_inner = (h - sqrtd) / a;
            if (!ray_t.contains(t_inner)) {
                t_inner = (h + sqrtd) / a;
            }
            if (ray_t.contains(t_inner) && (!hit_outer || t_inner < t)) {
                rec.t = t_inner;
                rec.p = r.at(t);
                normal = -(rec.p - center) / inner_radius;
                hit_outer = true;
            }
        }

        if (!hit_outer) return false;
        rec.set_face_normal(r, normal);
        auto outward_normal = (rec.p - center) / (normal.length() > 0 ? outer_radius : -inner_radius);
        rec.u = std::atan2(-rec.p.z, rec.p.x) / (2 * pi);
        rec.v = std::acos(-rec.p.y / (normal.length() > 0 ? outer_radius : inner_radius)) / pi;
        rec.mat = mat;
        return true;
    }

    void set_bounding_box() override {
        auto rvec = vec3(outer_radius, outer_radius, outer_radius);
        bbox = aabb(center - rvec, center + rvec);
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "SphericalShell(center=" << center << ", inner_radius=" << inner_radius << ", outer_radius=" << outer_radius << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    double inner_radius, outer_radius;
};

class rounded_box : public hittable {
public:
    rounded_box(const point3& a, const point3& b, double rounding_radius)
        : a(a), b(b), rounding_radius(std::fmin(std::fmax(0, rounding_radius), 0.5 * std::min({b.x - a.x, b.y - a.y, b.z - a.z}))) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        // Simplified SDF-based intersection (approximate)
        vec3 center = (a + b) / 2;
        vec3 extent = (b - a) / 2 - vec3(rounding_radius, rounding_radius, rounding_radius);
        vec3 oc = r.origin() - center;
        vec3 dir = r.direction();

        // Transform to box space
        vec3 abs_dir(std::fabs(dir.x), std::fabs(dir.y), std::fabs(dir.z));
        vec3 t_near = (-extent - oc) / dir;
        vec3 t_far = (extent - oc) / dir;

        double t_min = std::max({std::min(t_near.x, t_far.x), std::min(t_near.y, t_far.y), std::min(t_near.z, t_far.z)});
        double t_max = std::min({std::max(t_near.x, t_far.x), std::max(t_near.y, t_far.y), std::max(t_near.z, t_far.z)});

        if (t_min > t_max || !ray_t.contains(t_min)) return false;

        rec.t = t_min;
        rec.p = r.at(t_min);
        vec3 p = rec.p - center;
        vec3 n = vec3(
            p.x > extent.x ? 1 : (p.x < -extent.x ? -1 : 0),
            p.y > extent.y ? 1 : (p.y < -extent.y ? -1 : 0),
            p.z > extent.z ? 1 : (p.z < -extent.z ? -1 : 0)
        );
        rec.set_face_normal(r, unit_vector(n));
        rec.u = 0; // Simplified UV
        rec.v = 0;
        rec.mat = mat;
        return true;
    }

    void set_bounding_box() override {
        bbox = aabb(a, b);
    }

    void move_by(const point3& offset) override {
        a = a + offset;
        b = b + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "RoundedBox(a=" << a << ", b=" << b << ", rounding_radius=" << rounding_radius << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 a, b;
    double rounding_radius;
};

class infinite_cylinder : public hittable {
public:
    infinite_cylinder(const point3& base, const vec3& axis, double radius)
        : base(base), axis(unit_vector(axis)), radius(std::fmax(0, radius)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - base;
        vec3 dir = r.direction();
        vec3 n = axis;

        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n));
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - radius * radius;

        auto discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        auto t = (-b - sqrtd) / (2.0 * a);
        if (!ray_t.contains(t)) {
            t = (-b + sqrtd) / (2.0 * a);
            if (!ray_t.contains(t)) return false;
        }

        rec.t = t;
        rec.p = r.at(t);
        vec3 outward_normal = (rec.p - (base + dot(rec.p - base, n) * n)) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.u = std::atan2(dot(rec.p - base, cross(n, vec3(1,0,0))), dot(rec.p - base, cross(n, vec3(0,1,0)))) / (2 * pi);
        rec.v = 0; // No v for infinite cylinder
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        bbox = aabb(); // Infinite cylinder has no bounds
    }

    void move_by(const point3& offset) override {
        base = base + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "InfiniteCylinder(base=" << base << ", axis=" << axis << ", radius=" << radius << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 base;
    vec3 axis;
    double radius;
};

class paraboloid : public hittable {
public:
    paraboloid(const point3& vertex, const vec3& axis, double focal_length)
        : vertex(vertex), axis(unit_vector(axis)), focal_length(std::fmax(0, focal_length)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - vertex;
        vec3 dir = r.direction();
        vec3 n = axis;

        auto a = glm::length2(dir) - dot(dir, n) * dot(dir, n);
        auto b = 2.0 * (dot(oc, dir) - dot(oc, n) * dot(dir, n)) - focal_length * dot(dir, n);
        auto c = glm::length2(oc) - dot(oc, n) * dot(oc, n) - focal_length * dot(oc, n);

        auto discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        auto t = (-b - sqrtd) / (2.0 * a);
        if (!ray_t.contains(t)) {
            t = (-b + sqrtd) / (2.0 * a);
            if (!ray_t.contains(t)) return false;
        }

        rec.t = t;
        rec.p = r.at(t);
        vec3 outward_normal = (rec.p - vertex - focal_length * axis) / focal_length;
        rec.set_face_normal(r, outward_normal);
        rec.u = std::atan2(dot(rec.p - vertex, cross(n, vec3(1,0,0))), dot(rec.p - vertex, cross(n, vec3(0,1,0)))) / (2 * pi);
        rec.v = dot(rec.p - vertex, n) / (2 * focal_length);
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        // Approximate for a finite region
        auto rvec = vec3(focal_length, focal_length, focal_length);
        bbox = aabb(vertex - rvec, vertex + rvec);
    }

    void move_by(const point3& offset) override {
        vertex = vertex + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Paraboloid(vertex=" << vertex << ", axis=" << axis << ", focal_length=" << focal_length << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 vertex;
    vec3 axis;
    double focal_length;
};

class hyperboloid : public hittable {
public:
    hyperboloid(const point3& center, const vec3& axis, double a, double b, double c)
        : center(center), axis(unit_vector(axis)), a(std::fmax(0, a)), b(std::fmax(0, b)), c(std::fmax(0, c)) {
        set_bounding_box();
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 oc = r.origin() - center;
        vec3 dir = r.direction();
        vec3 n = axis;

        auto aa = (dir.x * dir.x) / (a * a) + (dir.y * dir.y) / (b * b) - (dir.z * dir.z) / (c * c);
        auto bb = 2.0 * ((oc.x * dir.x) / (a * a) + (oc.y * dir.y) / (b * b) - (oc.z * dir.z) / (c * c));
        auto cc = (oc.x * oc.x) / (a * a) + (oc.y * oc.y) / (b * b) - (oc.z * oc.z) / (c * c) - 1;

        auto discriminant = bb * bb - 4 * aa * cc;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);
        auto t = (-bb - sqrtd) / (2.0 * aa);
        if (!ray_t.contains(t)) {
            t = (-bb + sqrtd) / (2.0 * aa);
            if (!ray_t.contains(t)) return false;
        }

        rec.t = t;
        rec.p = r.at(t);
        vec3 p = rec.p - center;
        vec3 outward_normal = vec3(p.x / (a * a), p.y / (b * b), -p.z / (c * c));
        rec.set_face_normal(r, unit_vector(outward_normal));
        rec.u = std::atan2(p.y, p.x) / (2 * pi);
        rec.v = p.z / c;
        rec.mat = mat;

        return true;
    }

    void set_bounding_box() override {
        auto rvec = vec3(a, b, c);
        bbox = aabb(center - rvec, center + rvec);
    }

    void move_by(const point3& offset) override {
        center = center + offset;
        set_bounding_box();
    }

    std::ostream& print(std::ostream& out) const override {
        out << "Hyperboloid(center=" << center << ", axis=" << axis << ", a=" << a << ", b=" << b << ", c=" << c << ")";
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

private:
    point3 center;
    vec3 axis;
    double a, b, c;
};

#endif