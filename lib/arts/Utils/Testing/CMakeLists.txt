add_library(CartsTest STATIC
  CartsTest.c
)

set_target_properties(CartsTest PROPERTIES
  OUTPUT_NAME cartstest
)

install(TARGETS CartsTest
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)