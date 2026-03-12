# Add the main analysis source files
file(GLOB CARTS_ANALYSIS_SOURCE_FILES CONFIGURE_DEPENDS "*.cpp")

# Add the Db subdirectory
add_subdirectory(Db)

# Add the EDT subdirectory
add_subdirectory(Edt)

# Add the Graphs subdirectory
add_subdirectory(Graphs)

# Add the Metadata subdirectory
add_subdirectory(Metadata)

# Add the Loop subdirectory
add_subdirectory(Loop)

# Combine all analysis source files
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_DB_ANALYSIS_SOURCE_FILES})
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_EDT_ANALYSIS_SOURCE_FILES})
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_GRAPHS_SOURCE_FILES})
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_METADATA_ANALYSIS_SOURCE_FILES})
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${LOOP_ANALYSIS_SOURCE_FILES})

set(CARTS_ANALYSIS_SOURCE_FILES ${CARTS_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)
