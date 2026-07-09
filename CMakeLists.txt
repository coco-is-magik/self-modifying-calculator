cmake_minimum_required(VERSION 3.16)
project(smc LANGUAGES C VERSION 0.2.0)

# ---------------------------------------------------------------------------
# Install directories
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(SMC_BUILD_SHARED "Build libsmc as a shared library" ON)
option(SMC_BUILD_STATIC "Build libsmc as a static library" ON)
option(SMC_BUILD_EXAMPLES "Build C examples" ON)
option(SMC_BUILD_TESTS "Build C acceptance tests" ON)
option(SMC_BUILD_BENCHMARKS "Build C benchmarks" ON)
option(SMC_GENERATED_SOURCE "Path to a generated smc_generated.c dispatch table" "")
option(SMC_THREAD_SAFE "Enable internal locking for the global context" OFF)
option(SMC_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)
option(SMC_SANITIZE_MEMORY "Build with MemorySanitizer" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Symbol visibility
# ---------------------------------------------------------------------------
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
if(SMC_THREAD_SAFE)
    add_definitions(-DSMC_THREAD_SAFE=1)
endif()

if(SMC_SANITIZE_THREAD)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SMC_SANITIZE_THREAD requires GCC or Clang")
    endif()
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

if(SMC_SANITIZE_MEMORY)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "SMC_SANITIZE_MEMORY requires Clang")
    endif()
    add_compile_options(-fsanitize=memory -fno-omit-frame-pointer)
    add_link_options(-fsanitize=memory)
endif()

# ---------------------------------------------------------------------------
# Runtime library (Tier 1 + weak Tier 2 fallbacks)
# ---------------------------------------------------------------------------
set(SMC_RUNTIME_SOURCES
    src/c/smc_runtime_stub.c
    src/c/smc_artifact.c
    src/c/smc_state.c
)

if(SMC_BUILD_STATIC)
    add_library(smc_static STATIC ${SMC_RUNTIME_SOURCES})
    set_target_properties(smc_static PROPERTIES
        OUTPUT_NAME smc
        C_VISIBILITY_PRESET hidden
    )
    target_include_directories(smc_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(smc_static PUBLIC m)
endif()

if(SMC_BUILD_SHARED)
    # The shared runtime library includes the generated dispatch table directly
    # so that Python and other dynamic-language bindings can load a single
    # library and get both Tier 1 and Tier 2 implementations.
    set(SMC_SHARED_SOURCES ${SMC_RUNTIME_SOURCES})
    if(SMC_GENERATED_SOURCE)
        list(APPEND SMC_SHARED_SOURCES ${SMC_GENERATED_SOURCE})
    endif()
    add_library(smc_shared SHARED ${SMC_SHARED_SOURCES})
    set_target_properties(smc_shared PROPERTIES
        OUTPUT_NAME smc
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        C_VISIBILITY_PRESET hidden
    )
    target_include_directories(smc_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(smc_shared PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# Generated dispatch table (Tier 2 implementations)
# ---------------------------------------------------------------------------
if(SMC_GENERATED_SOURCE)
    if(NOT EXISTS "${SMC_GENERATED_SOURCE}")
        message(FATAL_ERROR "SMC_GENERATED_SOURCE does not exist: ${SMC_GENERATED_SOURCE}")
    endif()
    message(STATUS "SMC: linking generated dispatch table: ${SMC_GENERATED_SOURCE}")

    if(SMC_BUILD_STATIC)
        add_library(smc_generated_static STATIC ${SMC_GENERATED_SOURCE})
        set_target_properties(smc_generated_static PROPERTIES
            OUTPUT_NAME smc_generated
            C_VISIBILITY_PRESET hidden
        )
        target_include_directories(smc_generated_static PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(smc_generated_static PUBLIC m)
    endif()

    if(SMC_BUILD_SHARED)
        add_library(smc_generated_shared SHARED ${SMC_GENERATED_SOURCE})
        set_target_properties(smc_generated_shared PROPERTIES
            OUTPUT_NAME smc_generated
            VERSION ${PROJECT_VERSION}
            SOVERSION 1
            C_VISIBILITY_PRESET hidden
        )
        target_include_directories(smc_generated_shared PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(smc_generated_shared PUBLIC m)
    endif()
else()
    message(STATUS "SMC: no generated dispatch table linked; Tier 2 calls will return SMC_ERR_NOT_IMPL")
endif()

# ---------------------------------------------------------------------------
# Helper to link an executable against the right SMC libraries
# ---------------------------------------------------------------------------
function(smc_link_executable target)
    if(SMC_BUILD_STATIC AND SMC_GENERATED_SOURCE)
        # Static link order matters: generated symbols must override weak runtime stubs,
        # so smc_generated_static is linked after smc_static.
        target_link_libraries(${target} PRIVATE smc_generated_static smc_static)
    elseif(SMC_BUILD_SHARED AND SMC_GENERATED_SOURCE)
        target_link_libraries(${target} PRIVATE smc_generated_shared smc_shared)
    elseif(SMC_BUILD_STATIC)
        target_link_libraries(${target} PRIVATE smc_static)
    elseif(SMC_BUILD_SHARED)
        target_link_libraries(${target} PRIVATE smc_shared)
    else()
        message(FATAL_ERROR "At least one of SMC_BUILD_STATIC or SMC_BUILD_SHARED must be ON")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(SMC_BUILD_EXAMPLES)
    add_executable(hello_smc examples/c/hello_smc.c)
    smc_link_executable(hello_smc)

    add_executable(renderer_hotpath examples/c/renderer_hotpath.c)
    smc_link_executable(renderer_hotpath)

    add_executable(artifact_cache_basic examples/c/artifact_cache_basic.c)
    smc_link_executable(artifact_cache_basic)

    add_executable(dirty_state_basic examples/c/dirty_state_basic.c)
    smc_link_executable(dirty_state_basic)

    add_executable(glyph_block_cache examples/c/glyph_block_cache.c)
    smc_link_executable(glyph_block_cache)

    if(SMC_GENERATED_SOURCE)
        add_executable(renderer_ray examples/c/renderer_ray.c)
        smc_link_executable(renderer_ray)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SMC_BUILD_TESTS)
    enable_testing()

    add_executable(test_lifecycle tests/c/test_lifecycle.c)
    add_executable(test_parser tests/c/test_parser.c)
    add_executable(test_context_isolation tests/c/test_context_isolation.c)
    add_executable(test_metadata tests/c/test_metadata.c)
    add_executable(test_stats tests/c/test_stats.c)

    if(SMC_BUILD_STATIC)
        target_link_libraries(test_lifecycle PRIVATE smc_static)
        target_link_libraries(test_parser PRIVATE smc_static)
        target_link_libraries(test_context_isolation PRIVATE smc_static)
        target_link_libraries(test_metadata PRIVATE smc_static)
        target_link_libraries(test_stats PRIVATE smc_static)
    else()
        target_link_libraries(test_lifecycle PRIVATE smc_shared)
        target_link_libraries(test_parser PRIVATE smc_shared)
        target_link_libraries(test_context_isolation PRIVATE smc_shared)
        target_link_libraries(test_metadata PRIVATE smc_shared)
        target_link_libraries(test_stats PRIVATE smc_shared)
    endif()

    # test_metadata and test_stats use Tier 2 metadata helpers; link the
    # generated table when available so the symbols resolve.
    if(SMC_GENERATED_SOURCE)
        if(SMC_BUILD_STATIC)
            target_link_libraries(test_metadata PRIVATE smc_generated_static)
            target_link_libraries(test_stats PRIVATE smc_generated_static)
        else()
            target_link_libraries(test_metadata PRIVATE smc_generated_shared)
            target_link_libraries(test_stats PRIVATE smc_generated_shared)
        endif()
    endif()

    add_test(NAME test_lifecycle COMMAND test_lifecycle)
    add_test(NAME test_parser COMMAND test_parser)
    add_test(NAME test_context_isolation COMMAND test_context_isolation)
    add_test(NAME test_metadata COMMAND test_metadata)
    add_test(NAME test_stats COMMAND test_stats)

    add_executable(test_stub_runtime tests/c/test_stub_runtime.c)
    if(SMC_BUILD_STATIC)
        target_link_libraries(test_stub_runtime PRIVATE smc_static)
    else()
        target_link_libraries(test_stub_runtime PRIVATE smc_shared)
    endif()
    add_executable(test_artifact_cache tests/c/test_artifact_cache.c)
    if(SMC_BUILD_STATIC)
        target_link_libraries(test_artifact_cache PRIVATE smc_static)
    else()
        target_link_libraries(test_artifact_cache PRIVATE smc_shared)
    endif()
    add_test(NAME test_artifact_cache COMMAND test_artifact_cache)
    
    add_executable(test_state_tracking tests/c/test_state_tracking.c)
    if(SMC_BUILD_STATIC)
        target_link_libraries(test_state_tracking PRIVATE smc_static)
    else()
        target_link_libraries(test_state_tracking PRIVATE smc_shared)
    endif()
    add_test(NAME test_state_tracking COMMAND test_state_tracking)
    
    add_test(NAME test_stub_runtime COMMAND test_stub_runtime)

    if(SMC_GENERATED_SOURCE)
        add_executable(test_generated tests/c/test_generated.c)
        add_executable(test_generated_dispatch tests/c/test_generated_dispatch.c)
        add_executable(test_static_link tests/c/test_static_link.c)
        add_executable(test_shared_link tests/c/test_shared_link.c)
        smc_link_executable(test_generated)
        smc_link_executable(test_generated_dispatch)
        smc_link_executable(test_static_link)
        smc_link_executable(test_shared_link)
        add_test(NAME test_generated COMMAND test_generated)
        add_test(NAME test_generated_dispatch COMMAND test_generated_dispatch)
        add_test(NAME test_static_link COMMAND test_static_link)
        add_test(NAME test_shared_link COMMAND test_shared_link)
    endif()

    if(SMC_THREAD_SAFE)
        add_executable(test_thread_safety tests/c/test_thread_safety.c)
        if(SMC_BUILD_STATIC)
            target_link_libraries(test_thread_safety PRIVATE smc_static)
        else()
            target_link_libraries(test_thread_safety PRIVATE smc_shared)
        endif()
        if(SMC_GENERATED_SOURCE)
            if(SMC_BUILD_STATIC)
                target_link_libraries(test_thread_safety PRIVATE smc_generated_static)
            else()
                target_link_libraries(test_thread_safety PRIVATE smc_generated_shared)
            endif()
        endif()
        target_link_libraries(test_thread_safety PRIVATE pthread)
        add_test(NAME test_thread_safety COMMAND test_thread_safety)
    endif()

    # Regression: public symbols must be exported from the shared runtime.
    add_executable(test_symbol_visibility tests/c/test_symbol_visibility.c)
    if(SMC_BUILD_SHARED)
        target_link_libraries(test_symbol_visibility PRIVATE smc_shared)
    else()
        target_link_libraries(test_symbol_visibility PRIVATE smc_static)
    endif()
    add_test(NAME test_symbol_visibility COMMAND test_symbol_visibility)

    # Regression: all C runtime files must compile with strict -pedantic.
    add_library(smc_pedantic_check OBJECT
        src/c/smc_runtime_stub.c
        src/c/smc_artifact.c
        src/c/smc_state.c
        src/c/smc_generated_runtime.c
        src/c/smc_runtime_sbcl.c
    )
    target_include_directories(smc_pedantic_check PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_compile_options(smc_pedantic_check PRIVATE
        -Wall -Wextra -Werror -pedantic -std=c99
    )
    add_test(NAME test_pedantic_compile
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target smc_pedantic_check)
endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
if(SMC_BUILD_BENCHMARKS AND SMC_GENERATED_SOURCE)
    add_executable(benchmark_generated tests/benchmarks/benchmark_generated.c)
    smc_link_executable(benchmark_generated)

    add_executable(benchmark_dispatch_overhead tests/benchmarks/benchmark_dispatch_overhead.c)
    smc_link_executable(benchmark_dispatch_overhead)

    add_executable(benchmark_throughput tests/benchmarks/benchmark_throughput.c)
    smc_link_executable(benchmark_throughput)

    add_executable(benchmark_binary_size tests/benchmarks/benchmark_binary_size.c)
    smc_link_executable(benchmark_binary_size)

    add_executable(benchmark_cold_startup tests/benchmarks/benchmark_cold_startup.c)
    smc_link_executable(benchmark_cold_startup)
    
    add_executable(benchmark_artifact_state tests/benchmarks/benchmark_artifact_state.c)
    smc_link_executable(benchmark_artifact_state)
endif()

# ---------------------------------------------------------------------------
# Fuzz targets (not built by default; enable with SMC_BUILD_FUZZ=ON)
# ---------------------------------------------------------------------------
option(SMC_BUILD_FUZZ "Build fuzz targets (requires clang + libFuzzer)" OFF)
if(SMC_BUILD_FUZZ)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "SMC_BUILD_FUZZ requires Clang (for libFuzzer)")
    endif()

    add_executable(fuzz_parser tests/c/fuzz_parser.c)
    if(SMC_BUILD_STATIC)
        target_link_libraries(fuzz_parser PRIVATE smc_static)
    else()
        target_link_libraries(fuzz_parser PRIVATE smc_shared)
    endif()
    target_compile_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)

    if(SMC_GENERATED_SOURCE)
        add_executable(fuzz_generated tests/c/fuzz_generated.c)
        smc_link_executable(fuzz_generated)
        target_compile_options(fuzz_generated PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_generated PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endif()

# ---------------------------------------------------------------------------
# Sanitizer presets (not built by default; enable with SMC_SANITIZE=ON)
# ---------------------------------------------------------------------------
option(SMC_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
if(SMC_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# pkg-config
# ---------------------------------------------------------------------------
if(NOT WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/smc.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/smc.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/smc.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )

    if(SMC_GENERATED_SOURCE)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/smc-generated.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/smc-generated.pc"
            @ONLY
        )
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/smc-generated.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install(FILES include/smc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(SMC_BUILD_STATIC)
    install(TARGETS smc_static
        EXPORT smcTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

if(SMC_BUILD_SHARED)
    install(TARGETS smc_shared
        EXPORT smcTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(SMC_GENERATED_SOURCE)
    if(SMC_BUILD_STATIC)
        install(TARGETS smc_generated_static
            EXPORT smcTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
    if(SMC_BUILD_SHARED)
        install(TARGETS smc_generated_shared
            EXPORT smcTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

install(EXPORT smcTargets
    FILE smcTargets.cmake
    NAMESPACE smc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/smc
)

# Install examples
if(SMC_BUILD_EXAMPLES)
    if(SMC_GENERATED_SOURCE)
        install(TARGETS hello_smc renderer_hotpath renderer_ray
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    else()
        install(TARGETS hello_smc renderer_hotpath
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()
