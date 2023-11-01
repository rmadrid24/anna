cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "-g")
#set(PYTHON_EXECUTABLE /usr/bin/python3.8)

find_package (Threads REQUIRED)
find_package(OpenMP REQUIRED)
find_package(PkgConfig QUIET REQUIRED)
find_package(TBB REQUIRED REQUIRED)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(LIBPMEMKV REQUIRED libpmemkv)
else()
  message(FATAL_ERROR "pkg-config not found")
endif()

file(
        GLOB
	SRC_FILES
        "NvmMiddleware.cpp"
)

add_library(middleware STATIC ${SRC_FILES})
target_link_libraries(middleware ${LIBPMEMKV_LIBRARIES} hdr_histogram TBB::tbb)
ADD_DEPENDENCIES(middleware hdr_histogram)
