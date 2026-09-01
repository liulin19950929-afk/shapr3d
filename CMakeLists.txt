cmake_minimum_required(VERSION 3.16)
project(Shapr3DDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wno-unused-parameter -Wno-deprecated-declarations)

option(BUILD_APP "构建 GUI 应用程序" ON)
option(BUILD_TESTS "构建无头测试" ON)

# ---------- OCCT ----------
# 手动探测(兼容源码构建目录与系统安装), 避免不同版本 config 差异
set(OCCT_INC "" CACHE PATH "Open CASCADE 头文件目录")
set(OCCT_LIBDIR "" CACHE PATH "Open CASCADE 库目录")

if (NOT OCCT_INC)
    foreach(h
            ${OpenCASCADE_DIR}/include/opencascade
            $ENV{OpenCASCADE_DIR}/include/opencascade
            /usr/include/opencascade
            /usr/local/include/opencascade)
        if (EXISTS ${h}/gp_Pnt.hxx)
            set(OCCT_INC ${h})
            break()
        endif()
    endforeach()
endif()
if (NOT OCCT_LIBDIR)
    foreach(h
            ${OpenCASCADE_DIR}/lin64/gcc/lib
            $ENV{OpenCASCADE_DIR}/lin64/gcc/lib
            ${OpenCASCADE_DIR}/lib
            $ENV{OpenCASCADE_DIR}/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib)
        if (EXISTS ${h}/libTKernel.so OR EXISTS ${h}/TKernel.lib OR EXISTS ${h}/libTKernel.dylib)
            set(OCCT_LIBDIR ${h})
            break()
        endif()
    endforeach()
endif()

if (OCCT_INC AND OCCT_LIBDIR)
    message(STATUS "OCCT 头文件: ${OCCT_INC}")
    message(STATUS "OCCT 库目录: ${OCCT_LIBDIR}")
else()
    message(FATAL_ERROR "未找到 OCCT, 请用 -DOpenCASCADE_DIR=<occt构建/安装目录> 或 -DOCCT_INC= -DOCCT_LIBDIR=")
endif()

set(OCCT_TOOLKITS
    TKSTEP TKSTEP209 TKSTEPAttr TKSTEPBase TKDESTEP
    TKIGES TKDEIGES TKXSBase
    TKSTL TKDESTL TKDE
    TKMesh TKHLR
    TKFillet TKOffset TKShHealing TKBool TKBO TKTopAlgo TKPrim
    TKBRep TKGeomBase TKGeomAlgo TKG3d TKG2d TKMath TKernel)

set(OCCT_LIBS "")
foreach(tk ${OCCT_TOOLKITS})
    find_library(OCCT_${tk} ${tk} HINTS
        ${OCCT_LIBDIR}
        ${OpenCASCADE_DIR}
        ${OpenCASCADE_DIR}/lin64/gcc/lib
        ${OpenCASCADE_DIR}/lib
        /usr/lib/x86_64-linux-gnu)
    if (OCCT_${tk})
        list(APPEND OCCT_LIBS ${OCCT_${tk}})
    endif()
endforeach()
list(REMOVE_DUPLICATES OCCT_LIBS)
if (NOT OCCT_LIBS)
    message(FATAL_ERROR "找不到 OCCT 库, 请设置 -DOpenCASCADE_DIR=<路径>")
endif()

# ---------- 核心库(无 GL 依赖, 供测试与应用共用) ----------
add_library(cad_core STATIC
    src/core/Common.cpp
    src/kernel/Feature.cpp
    src/kernel/Document.cpp
    src/kernel/MeshBuilder.cpp
    src/sketch/SketchDef.cpp
    src/sketch/Solver.cpp
    src/drawing/Drawing.cpp
    src/drawing/DrawingExport.cpp
    src/io/Exchange.cpp
    src/analysis/Measure.cpp
    src/render/PngWriter.cpp)
target_include_directories(cad_core PUBLIC src ${OCCT_INC})
target_link_libraries(cad_core PUBLIC ${OCCT_LIBS})
find_package(Threads REQUIRED)
target_link_libraries(cad_core PUBLIC Threads::Threads)

# ---------- 第三方 ----------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)

set(APP_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/Ui.cpp
    src/app/ImGuiBackend.cpp)

if (BUILD_APP)
    if (NOT EXISTS ${IMGUI_DIR}/imgui.h)
        message(FATAL_ERROR "缺少 third_party/imgui, 请先运行 scripts/fetch-deps.sh")
    endif()

    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp)
    target_include_directories(imgui PUBLIC ${IMGUI_DIR})

    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLFW_DIR} glfw_build)

    add_executable(shapr3d ${APP_SOURCES})
    target_include_directories(shapr3d PRIVATE src ${GLFW_DIR}/include)
    target_compile_definitions(shapr3d PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(shapr3d PRIVATE cad_core imgui glfw)
    if (UNIX AND NOT APPLE)
        target_link_libraries(shapr3d PRIVATE dl)
    endif()
    set_target_properties(shapr3d PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
elseif (EXISTS ${IMGUI_DIR}/imgui.h AND EXISTS ${GLFW_DIR}/include/GLFW/glfw3.h)
    # 无头环境(如 CI 容器): 仅把 GUI 源码编译成对象以做语法检查, 不链接 GLFW
    add_library(cad_app_objs OBJECT ${APP_SOURCES})
    target_include_directories(cad_app_objs PRIVATE src ${GLFW_DIR}/include ${IMGUI_DIR})
    target_compile_definitions(cad_app_objs PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(cad_app_objs PRIVATE cad_core)
endif()

# ---------- 测试 ----------
if (BUILD_TESTS)
    enable_testing()
    add_executable(test_solver tests/test_solver.cpp)
    target_link_libraries(test_solver PRIVATE cad_core)
    add_executable(test_modeling tests/test_modeling.cpp)
    target_link_libraries(test_modeling PRIVATE cad_core)
    add_executable(test_drawing_io tests/test_drawing_io.cpp)
    target_link_libraries(test_drawing_io PRIVATE cad_core)
    add_executable(demo_headless tests/demo_headless.cpp)
    target_link_libraries(demo_headless PRIVATE cad_core)
    set_target_properties(test_solver test_modeling test_drawing_io demo_headless
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
