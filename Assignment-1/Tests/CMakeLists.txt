include_directories(../ ${GTRAV_DIR})
add_executable(ass1 test.cpp)
target_link_libraries(ass1 ${LIB} assign-1)
set_target_properties(ass1 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# loops over pta_assert_files and run "ass1-pta $bc_file"
file(GLOB pta_assert_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/testcases/pta/*.ll")
foreach(filename ${pta_assert_files})
    message(STATUS "Adding test for ${filename}")
    add_test(
            NAME ass1-pta/${filename}
            COMMAND ass1 -pta ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endforeach()

file(GLOB icfg_assert_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/testcases/icfg/*.ll")
foreach(filename ${icfg_assert_files})
    add_test(
            NAME ass1-icfg/${filename}
            COMMAND ass1 -icfg ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endforeach()

file(GLOB taint_assert_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/testcases/taint/*.ll")
foreach(filename ${taint_assert_files})
    add_test(
            NAME ass1-taint/${filename}
            COMMAND ass1 -taint ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endforeach()

