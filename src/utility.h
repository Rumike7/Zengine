#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <cstdlib>
#include <random>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_query.hpp>
#include <iostream>
#include <cmath>



#ifndef UTILITY_H
#define UTILITY_H

// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max-min)*random_double();
}

struct float3 {
    float x, y, z;
};

// Assume random_double is defined elsewhere
double random_double();
double random_double(double min, double max);

// Use GLM's vec3 instead of custom vec3 class
using vec3 = glm::vec3;

// point3 is an alias for glm::vec3
using point3 = glm::vec3;

// Vector Utility Functions

std::ostream& operator<<(std::ostream& out, const vec3& v) {
    out << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return out;
}


inline vec3 operator*(double t, const vec3& v) {
    return static_cast<float>(t) * v; // GLM scalar multiplication
}

inline vec3 operator*(const vec3& v, double t) {
    return t * v;
}

inline vec3 operator/(const vec3& v, double t) {
    return v / static_cast<float>(t); // GLM scalar division
}

inline vec3 operator/(const vec3& v, const vec3& u) {
    return v / u; // GLM component-wise division
}

inline bool operator==(const vec3& u, const vec3& v) {
    return glm::all(glm::equal(u, v)); // GLM equality check
}

inline bool operator!=(const vec3& u, const vec3& v) {
    return !glm::all(glm::equal(u, v)); // GLM inequality check
}

inline double dot(const vec3& u, const vec3& v) {
    return glm::dot(u, v); // GLM dot product
}

inline vec3 cross(const vec3& u, const vec3& v) {
    return glm::cross(u, v); // GLM cross product
}

inline vec3 unit_vector(const vec3& v) {
    return glm::normalize(v); // GLM normalization
}

inline vec3 random_unit_vector() {
    return glm::normalize(vec3(random_double(-1, 1), random_double(-1, 1), random_double(-1, 1)));
}

inline vec3 random_on_hemisphere(const vec3& normal) {
    vec3 on_unit_sphere = random_unit_vector();
    if (glm::dot(on_unit_sphere, normal) > 0.0f) {
        return on_unit_sphere;
    }
    return -on_unit_sphere;
}

inline vec3 reflect(const vec3& v, const vec3& n) {
    return glm::reflect(v, n); // GLM reflection
}

inline vec3 refract(const vec3& uv, const vec3& n, double etai_over_etat) {
    return glm::refract(uv, n, static_cast<float>(etai_over_etat)); // GLM refraction
}

inline vec3 random_in_unit_disk() {
    return glm::sphericalRand(1.0f); // GLM random in unit disk
}

inline int random_int(int min, int max) {
    return static_cast<int>(random_double(min, max + 1));
}

vec3 from_float3(const float3& f) {
    return vec3(f.x, f.y, f.z);
}

class mat3 {
public:
    mat3() : data{0} {}
    mat3(const vec3& col1, const vec3& col2, const vec3& col3) {
        data[0] = col1.x; data[3] = col2.x; data[6] = col3.x;
        data[1] = col1.y; data[4] = col2.y; data[7] = col3.y;
        data[2] = col1.z; data[5] = col2.z; data[8] = col3.z;
    }

    // Matrix-vector multiplication
    vec3 operator*(const vec3& v) const {
        return vec3(
            data[0] * v.x + data[3] * v.y + data[6] * v.z,
            data[1] * v.x + data[4] * v.y + data[7] * v.z,
            data[2] * v.x + data[5] * v.y + data[8] * v.z
        );
    }

    // Matrix inverse (using adjugate method)
    mat3 inverse() const {
        double det = data[0] * (data[4] * data[8] - data[7] * data[5]) -
                     data[3] * (data[1] * data[8] - data[7] * data[2]) +
                     data[6] * (data[1] * data[5] - data[4] * data[2]);
        if (std::abs(det) < 1e-8) {
            // Return identity matrix for singular case to avoid division by zero
            return mat3();
        }

        mat3 inv;
        inv.data[0] = (data[4] * data[8] - data[7] * data[5]) / det;
        inv.data[1] = (data[7] * data[2] - data[1] * data[8]) / det;
        inv.data[2] = (data[1] * data[5] - data[4] * data[2]) / det;
        inv.data[3] = (data[6] * data[5] - data[3] * data[8]) / det;
        inv.data[4] = (data[0] * data[8] - data[6] * data[2]) / det;
        inv.data[5] = (data[3] * data[2] - data[0] * data[5]) / det;
        inv.data[6] = (data[3] * data[7] - data[6] * data[4]) / det;
        inv.data[7] = (data[6] * data[1] - data[0] * data[7]) / det;
        inv.data[8] = (data[0] * data[4] - data[3] * data[1]) / det;
        return inv;
    }

    // Matrix transpose
    mat3 transpose() const {
        mat3 t;
        t.data[0] = data[0]; t.data[3] = data[1]; t.data[6] = data[2];
        t.data[1] = data[3]; t.data[4] = data[4]; t.data[7] = data[5];
        t.data[2] = data[6]; t.data[5] = data[7]; t.data[8] = data[8];
        return t;
    }

private:
    double data[9]; // Column-major storage
};

// Extension of vec3 to include missing methods
inline vec3 randomVec3() {
    return vec3(random_double(), random_double(), random_double());
}

inline vec3 randomVec3(double min, double max) {
    return vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

inline bool near_zero(const vec3& v) {
    const float s = 1e-8;
    return glm::all(glm::lessThan(glm::abs(v), vec3(s)));
}

inline float3 to_float3(const vec3& v) {
    return float3{v.x, v.y, v.z};
}


class material;

class ray {
  public:
    ray() {}

    ray(const point3& origin, const vec3& direction, double time)
      : orig(origin), dir(direction), tm(time) {}

    ray(const point3& origin, const vec3& direction)
      : ray(origin, direction, 0) {}

    const point3& origin() const  { return orig; }
    const vec3& direction() const { return dir; }

    double time() const { return tm; }

    point3 at(double t) const {
        return orig + t*dir;
    }

  private:
    point3 orig;
    vec3 dir;
    double tm;
};

std::ostream& operator<<(std::ostream& out, const ray& r) {
    out << "Ray(origin=" << r.origin() 
        << ", direction=" << r.direction() 
        << ", time=" << r.time() << ")";
    return out;
}


class hit_record {
  public:
    point3 p;
    vec3 normal;
    shared_ptr<material> mat;
    double t;
    double u;
    double v;
    bool front_face;

    void set_face_normal(const ray& r, const vec3& outward_normal) {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

#endif
