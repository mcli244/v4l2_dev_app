cmake_minimum_required(VERSION 3.0.0)

project(v4l2_dev_test)

set (TAGET v4l2_dev_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(inc)
set(SRCS
	main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/camera.cpp
	)

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/devlop)
set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")


set(EXECUTABLE_OUTPUT_PATH ${CMAKE_INSTALL_BINDIR}) 
set(LIBRARY_OUTPUT_PATH ${CMAKE_INSTALL_LIBDIR})
add_executable(${TAGET} ${SRCS})


target_link_libraries(${TAGET}
	pthread
)