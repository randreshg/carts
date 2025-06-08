# Add all db source files
file(GLOB CARTS_DB_ANALYSIS_SOURCE_FILES "*.cpp")

# Set the source files for parent scope
set(CARTS_DB_ANALYSIS_SOURCE_FILES ${CARTS_DB_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)

# Add the Graph subdirectory source files
add_subdirectory(Graph)

# Combine all db analysis source files
list(APPEND CARTS_DB_ANALYSIS_SOURCE_FILES ${CARTS_DB_ANALYSIS_GRAPH_SOURCE_FILES})

# Set the source files for parent scope
set(CARTS_DB_ANALYSIS_SOURCE_FILES ${CARTS_DB_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)
