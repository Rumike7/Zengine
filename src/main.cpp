#include <iostream>
#include <fstream>
#include "vec3.h"
#include "ray.h"
#include "sphere.h"
#include "hittable.h"

Vec3 ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, 0.0f, 1e30f, rec)) {
        // Simple diffuse shading based on normal
        return Vec3(0.5f * (rec.normal.x + 1), 0.5f * (rec.normal.y + 1), 0.5f * (rec.normal.z + 1));
    }
    // Background gradient
    Vec3 unit_dir = r.direction.normalized();
    float t = 0.5f * (unit_dir.y + 1.0f);
    return Vec3(1.0f, 1.0f, 1.0f) * (1.0f - t) + Vec3(0.5f, 0.7f, 1.0f) * t;
}

int main() {
    // Image setup
    const int width = 640;
    const int height = 480;

    // Scene setup
    Sphere sphere1(Vec3(0, 0, -1), 0.5f);  // Center sphere
    Sphere sphere2(Vec3(0, -100.5f, -1), 100);  // Large "ground" sphere

    // Camera setup
    Vec3 origin(0, 0, 0);
    float aspect_ratio = float(width) / height;
    float viewport_height = 2.0f;
    float viewport_width = aspect_ratio * viewport_height;
    float focal_length = 1.0f;
    Vec3 horizontal(viewport_width, 0, 0);
    Vec3 vertical(0, viewport_height, 0);
    Vec3 lower_left = origin - horizontal * 0.5f - vertical * 0.5f - Vec3(0, 0, focal_length);

    // Render to PPM file
    std::ofstream file("output/scene.ppm");
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = height - 1; j >= 0; --j) {
        std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
        for (int i = 0; i < width; ++i) {
            float u = float(i) / (width - 1);
            float v = float(j) / (height - 1);
            Ray r(origin, lower_left + horizontal * u + vertical * v - origin);
            Vec3 color = ray_color(r, sphere1) * 255.99f;  // Check sphere1 first
            if (!sphere1.hit(r, 0.0f, 1e30f, HitRecord())) {
                color = ray_color(r, sphere2) * 255.99f;  // Then sphere2
            }
            file << int(color.x) << " " << int(color.y) << " " << int(color.z) << "\n";
        }
    }

    file.close();
    std::cerr << "\nDone!\n";
    return 0;
}
