#include <iostream>
#include <fstream>
#include <cmath>

// Vec3 class
class Vec3 {
public:
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float t) const { return Vec3(x * t, y * t, z * t); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const { float len = length(); return len > 0 ? *this * (1 / len) : *this; }
};

// Ray class
class Ray {
public:
    Vec3 origin, direction;
    Ray() {}
    Ray(const Vec3& orig, const Vec3& dir) : origin(orig), direction(dir) {}
    Vec3 at(float t) const { return origin + direction * t; }
};

// Hittable base class and HitRecord struct
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

// Sphere class
class Sphere : public Hittable {
public:
    Vec3 center;
    float radius;

    Sphere(const Vec3& c, float r) : center(c), radius(r) {}

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        Vec3 oc = r.origin - center;
        float a = r.direction.dot(r.direction);
        float b = 2.0f * oc.dot(r.direction);
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
        rec.front_face = r.direction.dot(outward_normal) < 0;
        rec.normal = rec.front_face ? outward_normal : -outward_normal;
        return true;
    }
};

// Ray coloring function
Vec3 ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, 0.0f, 1e30f, rec)) {
        return Vec3(0.5f * (rec.normal.x + 1), 0.5f * (rec.normal.y + 1), 0.5f * (rec.normal.z + 1));
    }
    Vec3 unit_dir = r.direction.normalized();
    float t = 0.5f * (unit_dir.y + 1.0f);
    return Vec3(1.0f, 1.0f, 1.0f) * (1.0f - t) + Vec3(0.5f, 0.7f, 1.0f) * t;
}

// Main function
int main() {
    std::cerr << "Starting ray tracer...\n";

    // Image setup
    const int width = 640;
    const int height = 480;
    std::cerr << "Image size: " << width << "x" << height << "\n";

    // Scene setup
    Sphere sphere1(Vec3(0, 0, -1), 0.5f);
    Sphere sphere2(Vec3(0, -100.5f, -1), 100);
    std::cerr << "Scene initialized with 2 spheres\n";

    // Camera setup
    Vec3 origin(0, 0, 0);
    float aspect_ratio = float(width) / height;
    float viewport_height = 2.0f;
    float viewport_width = aspect_ratio * viewport_height;
    float focal_length = 1.0f;
    Vec3 horizontal(viewport_width, 0, 0);
    Vec3 vertical(0, viewport_height, 0);
    Vec3 lower_left = origin - horizontal * 0.5f - vertical * 0.5f - Vec3(0, 0, focal_length);
    std::cerr << "Camera initialized\n";

    // Render to PPM file
    std::ofstream file("output/scene.ppm");
    if (!file.is_open()) {
        std::cerr << "Failed to open output/scene.ppm\n";
        return 1;
    }
    std::cerr << "Output file opened\n";

    file << "P3\n" << width << " " << height << "\n255\n";
    std::cerr << "PPM header written\n";

    for (int j = height - 1; j >= 0; --j) {
        std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
        for (int i = 0; i < width; ++i) {
            float u = float(i) / (width - 1);
            float v = float(j) / (height - 1);
            Ray r(origin, lower_left + horizontal * u + vertical * v - origin);
            Vec3 color = ray_color(r, sphere1) * 255.99f;
            HitRecord rec;
            if (!sphere1.hit(r, 0.0f, 1e30f, rec)) {
                color = ray_color(r, sphere2) * 255.99f;
            }
            file << int(color.x) << " " << int(color.y) << " " << int(color.z) << "\n";
        }
    }

    file.close();
    std::cerr << "\nOutput file closed\n";
    std::cerr << "Done!\n";
    return 0;
}