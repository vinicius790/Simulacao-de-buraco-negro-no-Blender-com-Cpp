// SPDX-License-Identifier: MIT
/**
 * @file test_relativistic.cpp
 * @brief Unit tests for relativistic utilities.
 * Supports both standalone execution and GoogleTest runner when available.
 */

#if defined(BH_USE_GTEST) && __has_include(<gtest/gtest.h>)
#include <gtest/gtest.h>
#else
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int g_failures = 0;

inline void report_near(double val, double expected, double tol, const char* val_str, const char* exp_str) {
    const double diff = std::abs(val - expected);
    if (diff > tol) {
        std::cerr << "FAIL: " << val_str << " (" << val << ") != " << exp_str << " (" << expected
                  << ") [diff " << diff << " > tol " << tol << "]\n";
        ++g_failures;
    }
}

inline void report_double_eq(double val, double expected, const char* val_str, const char* exp_str) {
    if (val != expected && std::abs(val - expected) > 1e-15) {
        std::cerr << "FAIL: " << val_str << " (" << val << ") != " << exp_str << " (" << expected << ")\n";
        ++g_failures;
    }
}

struct RelTestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<RelTestCase>& get_tests() {
    static std::vector<RelTestCase> s_tests;
    return s_tests;
}
} // namespace

#define EXPECT_NEAR(val, expected, tol) report_near((val), (expected), (tol), #val, #expected)
#define EXPECT_DOUBLE_EQ(val, expected) report_double_eq((val), (expected), #val, #expected)

#define TEST(suite, name) \
    static void test_##suite##_##name(); \
    namespace { \
    struct reg_##suite##_##name { \
        reg_##suite##_##name() { get_tests().push_back({#suite "." #name, test_##suite##_##name}); } \
    } inst_##suite##_##name; \
    } \
    static void test_##suite##_##name()
#endif

#include "black_hole/relativistic.hpp"

namespace {

// Known values for a 10 solar mass black hole.
constexpr double solar_mass_kg = 1.98847e30;
constexpr double mass_kg = 10.0 * solar_mass_kg;

TEST(Relativistic, SchwarzschildRadius) {
    double rs = black_hole::schwarzschild_radius(mass_kg);
    // Expected rs = 2GM/c^2 ≈ 2.953 km per solar mass, so ~29.53 km for 10 M_sun.
    double expected = 2.0 * black_hole::G * mass_kg / (black_hole::c * black_hole::c);
    EXPECT_NEAR(rs, expected, 1e-6);
    EXPECT_NEAR(rs, 29.533e3, 1e2); // within 100 m tolerance of 29.533 km

    // C-linkage export helper
    EXPECT_DOUBLE_EQ(bh_schwarzschild_radius(mass_kg), rs);

    // Boundary conditions
    EXPECT_DOUBLE_EQ(black_hole::schwarzschild_radius(0.0), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::schwarzschild_radius(-100.0), 0.0);
}

TEST(Relativistic, LightDeflection) {
    double rs = black_hole::schwarzschild_radius(mass_kg);
    double impact = 10.0 * rs;
    double angle = black_hole::light_deflection_angle(mass_kg, impact);
    // Approximation: 4GM/(c^2 b) = 4*rs/(2*b) = 2*rs/b
    double expected = 2.0 * rs / impact;
    EXPECT_NEAR(angle, expected, 1e-12);

    // C-linkage export helper
    EXPECT_DOUBLE_EQ(bh_light_deflection_angle(mass_kg, impact), angle);

    // Boundary conditions
    EXPECT_DOUBLE_EQ(black_hole::light_deflection_angle(0.0, impact), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::light_deflection_angle(mass_kg, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::light_deflection_angle(mass_kg, -10.0), 0.0);
}

TEST(Relativistic, TimeDilation) {
    double rs = black_hole::schwarzschild_radius(mass_kg);
    double radius = 5.0 * rs; // outside horizon
    double factor = black_hole::time_dilation_factor(mass_kg, radius);
    double expected = std::sqrt(1.0 - rs / radius);
    EXPECT_NEAR(factor, expected, 1e-12);

    // C-linkage export helper
    EXPECT_DOUBLE_EQ(bh_time_dilation_factor(mass_kg, radius), factor);

    // Inside and on horizon should return 0
    EXPECT_DOUBLE_EQ(black_hole::time_dilation_factor(mass_kg, rs), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::time_dilation_factor(mass_kg, rs * 0.5), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::time_dilation_factor(mass_kg, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(black_hole::time_dilation_factor(0.0, radius), 0.0);
}

} // namespace

#if !defined(BH_USE_GTEST) || !__has_include(<gtest/gtest.h>)
int main() {
    std::cout << "Running test_relativistic (standalone test runner)\n";
    for (const auto& tc : get_tests()) {
        std::cout << "  [RUN] " << tc.name << "\n";
        tc.fn();
    }
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s) in test_relativistic\n";
        return EXIT_FAILURE;
    }
    std::cout << "All test_relativistic tests passed.\n";
    return EXIT_SUCCESS;
}
#endif
