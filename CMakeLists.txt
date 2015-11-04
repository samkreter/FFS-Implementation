cmake_minimum_required (VERSION 2.8.0)
project (FonefiveFS)


set(CMAKE_OSX_SYSROOT "/") # Mac users will thank me
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE ON)

set(CMAKE_C_FLAGS "-std=c99 -Wall -Werror -g -O0")

find_library(blockstore_lib block_store)

#find_library(bitmap_lib bitmap)
#find_library(dynarray_lib dyn_array)

#message(FATAL_ERROR "Copy your selected spec to include/${PROJECT_NAME}.h and delete this line!")

add_library(${PROJECT_NAME} SHARED src/${PROJECT_NAME}.c)
target_link_libraries(${PROJECT_NAME} ${blockstore_lib}) # Add other libs if you need them
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(FILES include/FonefiveFS_basic.h DESTINATION include)

add_executable(${PROJECT_NAME}_tester test/test.c)
target_link_libraries(${PROJECT_NAME}_tester ${blockstore_lib}) # Don't forget to add them here, too!
