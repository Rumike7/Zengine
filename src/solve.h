#include <cmath>
#include <algorithm>
#include <vector>

// Utility: Solve a quadratic ax^2 + bx + c = 0
int solve_quadratic(double a, double b, double c, double* roots) {
    if (std::abs(a) < 1e-8) {
        if (std::abs(b) < 1e-8) return 0;
        roots[0] = -c / b;
        return 1;
    }
    double D = b * b - 4 * a * c;
    if (D < 0) return 0;
    double sqrt_D = std::sqrt(D);
    roots[0] = (-b - sqrt_D) / (2 * a);
    roots[1] = (-b + sqrt_D) / (2 * a);
    return D > 0 ? 2 : 1;
}

// Utility: Solve depressed cubic x^3 + ax^2 + bx + c = 0
int solve_cubic(double a, double b, double c, double* roots) {
    double q = (3 * b - a * a) / 9;
    double r = (9 * a * b - 27 * c - 2 * a * a * a) / 54;
    double D = q * q * q + r * r;  // discriminant

    double offset = -a / 3.0;

    if (D >= 0) {
        // One real root
        double sqrt_D = std::sqrt(D);
        double s = std::cbrt(r + sqrt_D);
        double t = std::cbrt(r - sqrt_D);
        roots[0] = offset + s + t;
        return 1;
    } else {
        // Three real roots
        double theta = std::acos(r / std::sqrt(-q * q * q));
        double two_sqrt_q = 2 * std::sqrt(-q);
        roots[0] = offset + two_sqrt_q * std::cos(theta / 3);
        roots[1] = offset + two_sqrt_q * std::cos((theta + 2 * M_PI) / 3);
        roots[2] = offset + two_sqrt_q * std::cos((theta + 4 * M_PI) / 3);
        return 3;
    }
}

// Main: Solve quartic Ax^4 + Bx^3 + Cx^2 + Dx + E = 0
int solve_quartic(double A, double B, double C, double D, double E, double* roots) {
    if (std::abs(A) < 1e-8) {
        // Reduce to cubic
        return solve_cubic(B, C, D, roots);
    }

    // Normalize: x^4 + ax^3 + bx^2 + cx + d = 0
    double a = B / A, b = C / A, c = D / A, d = E / A;

    // Depressed quartic: x^4 + px^2 + qx + r = 0
    double a2 = a * a;
    double p = -3 * a2 / 8 + b;
    double q = a2 * a / 8 - a * b / 2 + c;
    double r = -3 * a2 * a2 / 256 + a2 * b / 16 - a * c / 4 + d;

    double cubic_roots[3];
    double coeffs[3] = {
        -p * p / 12 - r,
        -p * p * p / 108 + p * r / 3 - q * q / 8,
        -q * q / 8
    };

    int num_z = solve_cubic(0, coeffs[0], coeffs[1], cubic_roots);
    double z = cubic_roots[0];

    double u = std::sqrt(p + 2 * z);
    if (std::abs(u) < 1e-8) u = 0;

    double sub = a / 4;

    int n = 0;
    if (u != 0) {
        double quad1[2], quad2[2];
        int n1 = solve_quadratic(1, u, z - q / (2 * u), quad1);
        int n2 = solve_quadratic(1, -u, z + q / (2 * u), quad2);
        for (int i = 0; i < n1; ++i) roots[n++] = quad1[i] - sub;
        for (int i = 0; i < n2; ++i) roots[n++] = quad2[i] - sub;
    } else {
        double quad[2];
        int n1 = solve_quadratic(1, 0, z * z - r, quad);
        for (int i = 0; i < n1; ++i) roots[n++] = quad[i] - sub;
    }

    std::sort(roots, roots + n);
    return n;
}

vec3 rotate(const vec3& v, double angle, const vec3& axis) {
    // Normalize the axis
    vec3 a = unit_vector(axis);
    
    // Compute rotation matrix components
    double cos_theta = cos(angle);
    double sin_theta = sin(angle);
    double one_minus_cos = 1.0 - cos_theta;
    
    double x = a.x;
    double y = a.y;
    double z = a.z;
    
    // Rotation matrix
    double rot[3][3] = {
        {cos_theta + x*x*one_minus_cos,     x*y*one_minus_cos - z*sin_theta, x*z*one_minus_cos + y*sin_theta},
        {y*x*one_minus_cos + z*sin_theta,  cos_theta + y*y*one_minus_cos,   y*z*one_minus_cos - x*sin_theta},
        {z*x*one_minus_cos - y*sin_theta,  z*y*one_minus_cos + x*sin_theta,  cos_theta + z*z*one_minus_cos}
    };
    
    // Apply rotation
    return vec3(
        rot[0][0] * v.x + rot[0][1] * v.y + rot[0][2] * v.z,
        rot[1][0] * v.x + rot[1][1] * v.y + rot[1][2] * v.z,
        rot[2][0] * v.x + rot[2][1] * v.y + rot[2][2] * v.z
    );
}