add_subdirectory(opt)
add_subdirectory(run)

# Install the main carts wrapper script
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/carts
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
  COMPONENT carts
)