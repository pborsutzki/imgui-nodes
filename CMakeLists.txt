add_library(imgui_nodes STATIC
    "imgui_nodes.hpp"
    "imgui_nodes.cpp"
    "../cubicSplineClosestPoint/CubicSpline.cpp"
    "../cubicSplineClosestPoint/CubicSpline.h"
    "../cubicSplineClosestPoint/CubicSplineHelpers.h"
    "../cubicSplineClosestPoint/CubicSplineHelpers.inl"
    "../cubicSplineClosestPoint/CubicSplineSimd.inl"
    )
target_include_directories(imgui_nodes PUBLIC "." "../cubicSplineClosestPoint")
target_compile_definitions(imgui_nodes PRIVATE -DIMGUI_ANTIALIASFRINGESCALE)
target_link_libraries(imgui_nodes imgui)
set_target_properties(imgui_nodes PROPERTIES FOLDER tools)
