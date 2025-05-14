#ifndef QUAD_H
#define QUAD_H

#include "hittable.h"

class quad : public hittable {
  public:
    quad(const point3& Q, const vec3& u, const vec3& v, shared_ptr<material> mat, int id = -1)
      : Q(Q), u(u), v(v), mat(mat)
    {
        this->id = id;
        auto n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);
        w = n / dot(n,n);

        set_bounding_box();
    }

    void set_bounding_box() override {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }

    void move_by(const point3& offset) override {
        Q = Q + offset; 
        D = dot(normal, Q);
        set_bounding_box();
    }


    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction());

        // No hit if the ray is parallel to the plane.
        if (std::fabs(denom) < 1e-8)
            return false;

        // Return false if the hit point parameter t is outside the ray interval.
        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t))
            return false;

        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec))
            return false;


        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.set_face_normal(r, normal);

        return true;
    }

    virtual bool is_interior(double a, double b, hit_record& rec) const {
        interval unit_interval = interval(0, 1);
        // Given the hit point in plane coordinates, return false if it is outside the
        // primitive, otherwise set the hit record UV coordinates and return true.

        if (!unit_interval.contains(a) || !unit_interval.contains(b))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

    std::ostream& print(std::ostream& out) const override{
        out << "Quad("
            << "Q=" << Q
            << ", u=" << u
            << ", v=" << v << ")";
        return out;
    }
    
    std::istream& write(std::istream& in) const override{
        return in;
    }

  private:
    point3 Q;
    vec3 u, v;
    vec3 w;
    shared_ptr<material> mat;
    vec3 normal;
    double D;
};


class triangle : public quad {
public:
    triangle(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<material> mat, int id)
        : quad(Q, u, v, mat, id) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Triangle: valid if a >= 0, b >= 0, a + b <= 1
        if (a < 0 || b < 0 || a + b > 1) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

class rectangle : public quad {
public:
    rectangle(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<material> mat, int id)
        : quad(Q, u, v, mat, id) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Rectangle: same as quad (0 <= a,b <= 1)
        interval unit_interval(0, 1);
        if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

class disk : public quad {
public:
    disk(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<material> mat, int id)
        : quad(Q, u, v, mat, id) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Disk: valid if (a-0.5)^2 + (b-0.5)^2 <= 0.25 (radius 0.5 in normalized coords)
        double x = a - 0.5;
        double y = b - 0.5;
        if (x * x + y * y > 0.25) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

class ellipse : public quad {
public:
    ellipse(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<material> mat, int id)
        : quad(Q, u, v, mat, id) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Ellipse: valid if (a-0.5)^2/0.25 + (b-0.5)^2/0.16 <= 1 (semi-axes 0.5, 0.4)
        double x = (a - 0.5) / 0.5;
        double y = (b - 0.5) / 0.4;
        if (x * x + y * y > 1) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};


class ring : public quad {
public:
    ring(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<material> mat, int id)
        : quad(Q, u, v, mat, id) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Ring: valid if 0.04 <= (a-0.5)^2 + (b-0.5)^2 <= 0.25
        // Outer radius = 0.5, inner radius = 0.2 in normalized coords
        double x = a - 0.5;
        double y = b - 0.5;
        double r_squared = x * x + y * y;
        if (r_squared > 0.25 || r_squared < 0.04) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

class box : public hittable_list {
public:
    box(const point3& a, const point3& b, shared_ptr<material> mat, int id)
        : hittable_list(id), a(a), b(b), mat(mat) {
        // Compute minimum and maximum vertices
        point3 min(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
        point3 max(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

        // Define the vectors for the sides
        auto dx = vec3(max.x() - min.x(), 0, 0);
        auto dy = vec3(0, max.y() - min.y(), 0);
        auto dz = vec3(0, 0, max.z() - min.z());

        // Add the six quad faces
        objects.push_back(make_shared<quad>(point3(min.x(), min.y(), max.z()), dx, dy, mat)); // front
        objects.push_back(make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz, dy, mat)); // right
        objects.push_back(make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx, dy, mat)); // back
        objects.push_back(make_shared<quad>(point3(min.x(), min.y(), min.z()), dz, dy, mat)); // left
        objects.push_back(make_shared<quad>(point3(min.x(), max.y(), max.z()), dx, -dz, mat)); // top
        objects.push_back(make_shared<quad>(point3(min.x(), min.y(), min.z()), dx, dz, mat)); // bottom

        set_bounding_box();
    }

    // Optional: Method to move the box by translating all vertices
    void move_by(const vec3& offset) {
        a = a + offset;
        b = b + offset;
        // Reconstruct the box with new positions
        for(auto &obj : objects){
            obj->move_by(offset);
        }
        set_bounding_box();
    }

    void set_bounding_box() override {
        bbox = aabb(a, b);
    }

    std::istream& write(std::istream& in) const override{
        return in;
    }


    point3 a; // First opposite vertex
    point3 b; // Second opposite vertex
private:
    shared_ptr<material> mat; // Material for all faces
};

std::ostream& operator<<(std::ostream& out, const box& b) {
    out << "Box("
        << "a=" << b.a
        << ", b=" << b.b
        << ")";
    return out;
}


class grid : public hittable {
public:
    grid(int size = 10, double spacing = 1.0, const color& grid_color = color(0.8, 0.8, 0.8))
        : size(size), spacing(spacing), m_color(grid_color) {
        // Create a non-hit material for the grid
        grid_material = std::make_shared<lambertian>(grid_color);
    }

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        return false;
    }

    virtual aabb bounding_box() const override {
        double extent = size * spacing;
        return aabb(point3(-extent/2, -0.001, -extent/2), point3(extent/2, 0.001, extent/2));
    }

    void render(const hittable& world, int samples_per_pixel, int max_depth) {
        // This method would be called specifically to render the grid lines
        // It's separate from the normal rendering path
    }

    std::vector<std::shared_ptr<quad>> get_lines() const {
        std::vector<std::shared_ptr<quad>> lines;
        
        // Create the grid lines
        double half_size = size * spacing * 0.5;
        double line_width = 0.02; // Thin lines
        double y_level = 0.001;   // Slightly above ground level
        
        // Create x-axis lines (running along x, varying in z)
        for (int i = -size/2; i <= size/2; i++) {
            double z = i * spacing;
            if (i == 0) continue; // Skip center line since we'll make it special
            
            // Create a thin quad for each line
            point3 Q(half_size, y_level, z - line_width/2);
            vec3 u(-2*half_size, 0, 0);
            vec3 v(0, 0, line_width);
            
            // Create a line with the grid material
            auto line = std::make_shared<quad>(Q, u, v, grid_material, -1);
            lines.push_back(line);
        }
        
        // Create z-axis lines (running along z, varying in x)
        for (int i = -size/2; i <= size/2; i++) {
            double x = i * spacing;
            if (i == 0) continue; // Skip center line since we'll make it special
            
            // Create a thin quad for each line
            point3 Q(x - line_width/2, y_level, half_size);
            vec3 u(line_width, 0, 0);
            vec3 v(0, 0, -2*half_size);
            
            // Create a line with the grid material
            auto line = std::make_shared<quad>(Q, u, v, grid_material, -1);
            lines.push_back(line);
        }
        
        // Create X and Z axes with different colors (red for X, blue for Z)
        auto x_axis_material = std::make_shared<lambertian>(color(0.9, 0.2, 0.2)); // Red for X
        auto z_axis_material = std::make_shared<lambertian>(color(0.2, 0.2, 0.9)); // Blue for Z
        
        // X-axis (red)
        point3 x_Q(-half_size, y_level, -line_width);
        vec3 x_u(2*half_size, 0, 0);
        vec3 x_v(0, 0, 2*line_width);
        auto x_axis = std::make_shared<quad>(x_Q, x_u, x_v, x_axis_material, -1);
        lines.push_back(x_axis);
        
        // Z-axis (blue)
        point3 z_Q(-line_width, y_level, -half_size);
        vec3 z_u(2*line_width, 0, 0);
        vec3 z_v(0, 0, 2*half_size);
        auto z_axis = std::make_shared<quad>(z_Q, z_u, z_v, z_axis_material, -1);
        lines.push_back(z_axis);
        
        return lines;
    }

    std::ostream& print(std::ostream& out) const override{
        return out;
    }

    std::istream& write(std::istream& in) const override{
        return in;
    }

private:
    int size;                  // Number of grid lines in each direction
    double spacing;            // Distance between grid lines
    color m_color;             // Grid color
    std::shared_ptr<material> grid_material;
};

#endif
