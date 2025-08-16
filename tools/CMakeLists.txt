add_subdirectory(opt)
add_subdirectory(run)
add_subdirectory(visualizer)

# Install the main carts wrapper script into the same bin as other tools
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/carts
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION ${LLVM_TOOLS_INSTALL_DIR}
  COMPONENT carts
)