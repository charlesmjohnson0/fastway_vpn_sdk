cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "fy_vpn_sdk")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "fy_vpn_sdk_plugin")

add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(_WIN32_WINNT=0x0602)

set(LIB_FASTWAY ${PROJECT_SOURCE_DIR}\\..\\fastway_protocol\\)

include_directories(${LIB_FASTWAY}include)
add_subdirectory(${LIB_FASTWAY} fastway_protocol)

add_library(${PLUGIN_NAME} SHARED
  "fy_vpn_sdk_plugin.cpp"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin fastway_a)

# List of absolute paths to libraries that should be bundled with the plugin
set(fy_vpn_sdk_bundled_libraries
  ""
  PARENT_SCOPE
)
