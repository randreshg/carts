# Add the edt analysis source files
file(GLOB CARTS_EDT_ANALYSIS_SOURCE_FILES "*.cpp")

# Add the Graph subdirectory source files
add_subdirectory(Graph)
file(GLOB CARTS_EDT_GRAPH_SOURCE_FILES "Graph/*.cpp")

# Combine all edt analysis source files
list(APPEND CARTS_EDT_ANALYSIS_SOURCE_FILES ${CARTS_EDT_GRAPH_SOURCE_FILES})

# Set the source files for parent scope
set(CARTS_EDT_ANALYSIS_SOURCE_FILES ${CARTS_EDT_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)
