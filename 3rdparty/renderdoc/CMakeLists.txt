add_library(renderdoc_api INTERFACE)

target_include_directories(renderdoc_api INTERFACE include)

add_library(renderdoc::api ALIAS renderdoc_api)