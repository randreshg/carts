add_library(CartsBenchmarks STATIC
  CartsBenchmarks.c
)

set_target_properties(CartsBenchmarks PROPERTIES
  OUTPUT_NAME cartsbenchmarks
)

install(TARGETS CartsBenchmarks
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)