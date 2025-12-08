add_subdirectory(arts)

# Install CARTS test utilities header
install(FILES arts/Utils/Testing/CartsTest.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arts/Utils/Testing
)
