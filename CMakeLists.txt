cmake_minimum_required(VERSION 3.21)
project(BlackHoleSim VERSION 0.8.0 LANGUAGES CXX)

# Must come before any add_test() (the GL block registers gpu_cpu_agreement).
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BLACK_HOLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BLACK_HOLE_BUILD_GL "Build OpenGL visualization targets (BlackHole2D/3D)" ON)

function(black_hole_configure_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(BLACK_HOLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
        if(BLACK_HOLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Scientific reference library (always built; no OpenGL deps)
# ---------------------------------------------------------------------------
add_library(bh_scientific
    src/scientific/schwarzschild.cpp
    src/scientific/ray_state.cpp
    src/scientific/conserved.cpp
    src/scientific/integrator_rk4.cpp
    src/scientific/integrator_rk45.cpp
    src/scientific/planar_geodesic.cpp
    src/scientific/scene_params.cpp
    src/scientific/cpu_renderer.cpp
    src/scientific/image_io.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(bh_scientific PUBLIC Threads::Threads)
target_include_directories(bh_scientific
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
black_hole_configure_target(bh_scientific)

add_executable(test_scientific_ref tests/test_scientific_ref.cpp)
target_link_libraries(test_scientific_ref PRIVATE bh_scientific)
black_hole_configure_target(test_scientific_ref)

add_executable(quickstart_scientific examples/quickstart_scientific.cpp)
target_link_libraries(quickstart_scientific PRIVATE bh_scientific)
black_hole_configure_target(quickstart_scientific)

add_executable(bh_scene_dump tools/bh_scene_dump.cpp)
target_link_libraries(bh_scene_dump PRIVATE bh_scientific)
black_hole_configure_target(bh_scene_dump)

# Headless CPU renderer (reference images, golden tests, Blender render bridge).
add_executable(bh_render_cpu tools/bh_render_cpu.cpp)
target_link_libraries(bh_render_cpu PRIVATE bh_scientific)
black_hole_configure_target(bh_render_cpu)

add_executable(test_cpu_render tests/test_cpu_render.cpp)
target_link_libraries(test_cpu_render PRIVATE bh_scientific)
black_hole_configure_target(test_cpu_render)

# ---------------------------------------------------------------------------
# Optional OpenGL visualization targets
# ---------------------------------------------------------------------------
if(BLACK_HOLE_BUILD_GL)
    find_package(GLEW REQUIRED)

    # glfw3: prefer CONFIG (vcpkg / modern distros), fall back to module / pkg hints
    find_package(glfw3 CONFIG QUIET)
    if(NOT glfw3_FOUND AND NOT TARGET glfw)
        find_package(glfw3 QUIET)
    endif()
    if(NOT glfw3_FOUND AND NOT TARGET glfw)
        find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h)
        find_library(GLFW_LIBRARY NAMES glfw glfw3)
        if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
            add_library(glfw UNKNOWN IMPORTED)
            set_target_properties(glfw PROPERTIES
                IMPORTED_LOCATION "${GLFW_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
            )
            set(glfw3_FOUND TRUE)
            message(STATUS "glfw3: using find_path/find_library fallback")
        endif()
    endif()
    if(NOT TARGET glfw AND NOT glfw3_FOUND)
        message(FATAL_ERROR "glfw3 not found (tried CONFIG, module, and path/library fallback)")
    endif()

    # glm: CONFIG first, then header-only include
    find_package(glm CONFIG QUIET)
    if(NOT TARGET glm::glm)
        find_path(GLM_INCLUDE_DIR glm/glm.hpp)
        if(GLM_INCLUDE_DIR)
            add_library(glm::glm INTERFACE IMPORTED)
            set_target_properties(glm::glm PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${GLM_INCLUDE_DIR}"
            )
            message(STATUS "glm: using header-only fallback at ${GLM_INCLUDE_DIR}")
        else()
            message(FATAL_ERROR "glm not found (tried CONFIG and glm/glm.hpp)")
        endif()
    endif()

    find_package(OpenGL REQUIRED)

    set(DEPS glfw GLEW::GLEW glm::glm OpenGL::GL)

    add_executable(BlackHole2D 2D_lensing.cpp)
    target_link_libraries(BlackHole2D PRIVATE ${DEPS})
    target_include_directories(BlackHole2D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    black_hole_configure_target(BlackHole2D)

    add_executable(BlackHole3D black_hole.cpp)
    # bh_scientific provides --scene JSON loading and the scientific-mode unit scaling.
    target_link_libraries(BlackHole3D PRIVATE ${DEPS} bh_scientific)
    target_include_directories(BlackHole3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    black_hole_configure_target(BlackHole3D)

    set(BASELINE_SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/geodesic.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/grid.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/grid.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geodesic_scientific.comp
    )
    foreach(shader IN LISTS BASELINE_SHADERS)
        add_custom_command(TARGET BlackHole3D POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${shader}
                $<TARGET_FILE_DIR:BlackHole3D>
            VERBATIM
        )
    endforeach()

    # Real GPU vs CPU golden agreement (runs under xvfb-run/Mesa when no display;
    # reports SKIP via return code 77 when no OpenGL 4.3 context is available).
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(
                NAME gpu_cpu_agreement
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gpu_cpu_agreement.py
                    --root ${CMAKE_CURRENT_SOURCE_DIR}
                    --bin-dir $<TARGET_FILE_DIR:BlackHole3D>
            )
            set_tests_properties(gpu_cpu_agreement PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)
            add_test(
                NAME legacy_golden
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_legacy_golden.py
                    --root ${CMAKE_CURRENT_SOURCE_DIR}
                    --bin-dir $<TARGET_FILE_DIR:BlackHole3D>
            )
            set_tests_properties(legacy_golden PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)
            add_test(
                NAME bh3d_cli
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bh3d_cli.py
                    --root ${CMAKE_CURRENT_SOURCE_DIR}
                    --bin-dir $<TARGET_FILE_DIR:BlackHole3D>
            )
            set_tests_properties(bh3d_cli PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)
        endif()
    endif()

    find_program(GLSLANG_VALIDATOR NAMES glslangValidator)
    if(GLSLANG_VALIDATOR)
        add_custom_target(validate_shaders
            COMMAND ${GLSLANG_VALIDATOR} ${CMAKE_CURRENT_SOURCE_DIR}/grid.vert
            COMMAND ${GLSLANG_VALIDATOR} ${CMAKE_CURRENT_SOURCE_DIR}/grid.frag
            COMMAND ${GLSLANG_VALIDATOR} ${CMAKE_CURRENT_SOURCE_DIR}/geodesic.comp
            COMMAND ${GLSLANG_VALIDATOR} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geodesic_scientific.comp
            COMMENT "Validating baseline + scientific GLSL shaders"
            VERBATIM
        )
    endif()
else()
    message(STATUS "BLACK_HOLE_BUILD_GL=OFF — building scientific library and tests only")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME source_invariants
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_source_invariants.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test(
            NAME style_contract
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_style_contract.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test(
            NAME blender_addon_compile
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_blender_addon_compile.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test(
            NAME blender_addon_parity
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_blender_addon_parity.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test(
            NAME tools_selftest
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tools.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test(
            NAME shader_contract
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shader_contract.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()

    add_test(NAME scientific_ref COMMAND test_scientific_ref)
    add_test(NAME cpu_render COMMAND test_cpu_render)
    add_test(
        NAME render_cli_smoke
        COMMAND bh_render_cpu --scene ${CMAKE_CURRENT_SOURCE_DIR}/examples/scene_params_example.json
                --out ${CMAKE_CURRENT_BINARY_DIR}/smoke_legacy.png --width 48 --height 36 --quiet
    )
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME render_cli_contract
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_cli.py
                --root ${CMAKE_CURRENT_SOURCE_DIR} --bin $<TARGET_FILE:bh_render_cpu>
        )
    endif()
    add_test(
        NAME render_cli_rejects_bad_spin
        COMMAND bh_render_cpu --out ${CMAKE_CURRENT_BINARY_DIR}/never.png --spin-sign 0.5 --quiet
    )
    set_tests_properties(render_cli_rejects_bad_spin PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME render_cli_relativistic_sequence
        COMMAND bh_render_cpu --out ${CMAKE_CURRENT_BINARY_DIR}/smoke_rel.bmp --width 32 --height 24
                --mode relativistic --integrator rk45 --frames 2 --quiet
    )
endif()
