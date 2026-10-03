#pragma once
// Continuous (segment-based) hit tests replacing the legacy point-sampling.
//
// The legacy geodesic.comp detects a disk hit by checking the SIGN CHANGE of y
// between two samples and then evaluating ρ at the *new* sample. For thin
// geometry that mis-places the hit by up to one step. These helpers
// interpolate the crossing point along the straight segment between two
// consecutive samples (second-order accurate in the step size).

#include <array>
#include <cmath>

namespace bh {
namespace hit {

using Vec3 = std::array<double, 3>;

inline double norm3(const Vec3& a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }

inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) {
    return {{a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}};
}

/// Segment–plane crossing of the plane {axis = 0} (axis: 0=x, 1=y, 2=z).
/// Returns true if p0 and p1 lie on strictly opposite sides (or p1 on the plane);
/// `t_out` is the parametric crossing position in [0,1], `hit_out` the point.
inline bool segment_plane_crossing(const Vec3& p0, const Vec3& p1, int axis,
                                   double& t_out, Vec3& hit_out) {
    const double a = p0[axis];
    const double b = p1[axis];
    if (a == b) {
        return false;
    }
    if (a * b > 0.0) {
        return false;
    }
    t_out = a / (a - b);
    if (t_out < 0.0) t_out = 0.0;
    if (t_out > 1.0) t_out = 1.0;
    hit_out = lerp(p0, p1, t_out);
    hit_out[axis] = 0.0;
    return true;
}

/// Equatorial annulus test for a crossing point on the plane normal to `axis`.
/// Returns cylindrical radius ρ (distance from the axis) in rho_out.
inline bool point_in_annulus(const Vec3& p, int axis, double r_inner, double r_outer, double& rho_out) {
    double sum = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (i == axis) continue;
        sum += p[i] * p[i];
    }
    rho_out = std::sqrt(sum);
    return rho_out >= r_inner && rho_out <= r_outer;
}

/// Segment–sphere intersection. Returns true if the segment [p0,p1] intersects
/// the sphere (center c, radius R), with the smallest parametric t in [0,1].
/// A segment starting inside the sphere reports t = 0.
inline bool segment_sphere_hit(const Vec3& p0, const Vec3& p1, const Vec3& c, double R,
                               double& t_out) {
    const Vec3 d{{p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}};
    const Vec3 m{{p0[0] - c[0], p0[1] - c[1], p0[2] - c[2]}};
    const double mm = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
    if (mm <= R * R) {
        t_out = 0.0;
        return true;
    }
    const double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (dd <= 0.0) {
        return false;
    }
    const double md = m[0] * d[0] + m[1] * d[1] + m[2] * d[2];
    const double cc = mm - R * R;
    const double disc = md * md - dd * cc;
    if (disc < 0.0) {
        return false;
    }
    const double t = (-md - std::sqrt(disc)) / dd;
    if (t < 0.0 || t > 1.0) {
        return false;
    }
    t_out = t;
    return true;
}

}  // namespace hit
}  // namespace bh
