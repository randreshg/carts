add_library(CartsBenchmarks STATIC
  CartsBenchmarks.cpp
)

# Compile with -O0 to prevent empty parallel region optimization
target_compile_options(CartsBenchmarks PRIVATE -O0)

# Enable OpenMP for thread pool warmup
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(CartsBenchmarks PRIVATE OpenMP::OpenMP_CXX)
  target_compile_definitions(CartsBenchmarks PRIVATE _OPENMP)
endif()

set_target_properties(CartsBenchmarks PROPERTIES
  OUTPUT_NAME cartsbenchmarks
)

install(TARGETS CartsBenchmarks
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${CMAKE_SOURCE_DIR}/include/arts/Utils/Benchmarks/CartsBenchmarks.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arts/Utils/Benchmarks
)