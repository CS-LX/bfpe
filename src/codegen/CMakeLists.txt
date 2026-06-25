add_library(bfpe_codegen STATIC
    parse_sig.cpp
    bf2asm.cpp
    bf_interp.cpp
)

target_include_directories(bfpe_codegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(bfpe_codegen PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(bfpe_codegen PRIVATE /W4 /utf-8)
endif()

add_executable(bfpe_codegen_test test_codegen.cpp)
target_link_libraries(bfpe_codegen_test PRIVATE bfpe_codegen)
target_compile_features(bfpe_codegen_test PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(bfpe_codegen_test PRIVATE /W4 /utf-8)
endif()

set_target_properties(bfpe_codegen_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME bfpe_codegen_test COMMAND bfpe_codegen_test)
