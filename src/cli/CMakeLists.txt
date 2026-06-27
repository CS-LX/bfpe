find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(BFPE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BFPE_GENERATED_DIR}")

set(BFPE_EMBED_INPUTS
    "${CMAKE_SOURCE_DIR}/runtime/bf_abi.h"
    "${CMAKE_SOURCE_DIR}/runtime/bf_export_runtime.c"
    "${CMAKE_SOURCE_DIR}/runtime/bf_export_runtime.h"
    "${CMAKE_SOURCE_DIR}/runtime/bf_io.c"
    "${CMAKE_SOURCE_DIR}/runtime/bf_io.h"
    "${CMAKE_SOURCE_DIR}/runtime/bf_stub.c"
    "${CMAKE_SOURCE_DIR}/runtime/dllmain.c"
    "${CMAKE_SOURCE_DIR}/runtime/vm/bf_vm.c"
    "${CMAKE_SOURCE_DIR}/runtime/vm/bf_vm.h"
    "${CMAKE_SOURCE_DIR}/tools/generate_embedded_bundle.py"
)

add_custom_command(
    OUTPUT
        "${BFPE_GENERATED_DIR}/embedded_bundle.gen.cpp"
        "${BFPE_GENERATED_DIR}/embedded_bundle.gen.hpp"
    COMMAND
        ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tools/generate_embedded_bundle.py"
        --root "${CMAKE_SOURCE_DIR}"
        --out-dir "${BFPE_GENERATED_DIR}"
    DEPENDS ${BFPE_EMBED_INPUTS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
)

add_executable(bfpe
    main.cpp
    paths.cpp
    bundle.cpp
    process.cpp
    msvc_env.cpp
    manifest.cpp
    verify_pe.cpp
    build_cmd.cpp
    run_cmd.cpp
    exec_cmd.cpp
    cli.cpp
    "${BFPE_GENERATED_DIR}/embedded_bundle.gen.cpp"
)

add_custom_target(bfpe_embedded_bundle DEPENDS
    "${BFPE_GENERATED_DIR}/embedded_bundle.gen.cpp"
    "${BFPE_GENERATED_DIR}/embedded_bundle.gen.hpp"
)
add_dependencies(bfpe bfpe_embedded_bundle)

target_include_directories(bfpe PRIVATE "${BFPE_GENERATED_DIR}")

target_link_libraries(bfpe PRIVATE bfpe_codegen shell32)

target_compile_features(bfpe PRIVATE cxx_std_17)
target_compile_definitions(bfpe PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(bfpe PRIVATE /W4 /utf-8)
endif()

set_target_properties(bfpe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
