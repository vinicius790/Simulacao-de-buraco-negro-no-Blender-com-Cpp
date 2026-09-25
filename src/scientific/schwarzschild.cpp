#include "black_hole/schwarzschild.hpp"

#include <cmath>

namespace bh {

double lapse_factor(double r, double rs) {
    if (r <= rs) {
        return 0.0;
    }
    return 1.0 - rs / r;
}

MetricComponents metric_at(double r, double theta, double rs) {
    MetricComponents g{};
    g.f = lapse_factor(r, rs);
    g.g_tt = -g.f;
    g.g_rr = (g.f > 0.0) ? (1.0 / g.f) : 0.0;
    g.g_thth = r * r;
    const double s = std::sin(theta);
    g.g_phph = r * r * s * s;
    return g;
}

MetricComponents inverse_metric_at(double r, double theta, double rs) {
    MetricComponents g = metric_at(r, theta, rs);
    MetricComponents inv{};
    inv.f = g.f;
    inv.g_tt = (g.f > 0.0) ? (-1.0 / g.f) : 0.0;
    inv.g_rr = g.f;
    inv.g_thth = (r != 0.0) ? (1.0 / (r * r)) : 0.0;
    const double s = std::sin(theta);
    const double s2 = s * s;
    inv.g_phph = (r != 0.0 && s2 > 0.0) ? (1.0 / (r * r * s2)) : 0.0;
    return inv;
}

}  // namespace bh
