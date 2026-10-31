add_library(riag_rs STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/riag_rs.cpp
    ${RIAG_RS_BINDINGS_DIR}/sources/riag/src/lib.rs.cc
)

target_include_directories(riag_rs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${RIAG_RS_BINDINGS_DIR}/include
        ${RIAG_RS_BINDINGS_DIR}/crate
        ${RIAG_RS_BINDINGS_DIR}/sources
)

target_link_libraries(riag_rs riag)
