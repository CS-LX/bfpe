add_executable(bfpe
    main.cpp
    paths.cpp
    process.cpp
    msvc_env.cpp
    manifest.cpp
    build_cmd.cpp
    run_cmd.cpp
    exec_cmd.cpp
    cli.cpp
)

target_link_libraries(bfpe PRIVATE bfpe_codegen)

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
