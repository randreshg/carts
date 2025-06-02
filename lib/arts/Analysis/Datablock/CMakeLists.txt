# Add the datablock analysis source files
file(GLOB CARTS_DATABLOCK_ANALYSIS_SOURCE_FILES "*.cpp")

# Add the Graph subdirectory source files
add_subdirectory(Graph)
file(GLOB CARTS_DATABLOCK_GRAPH_SOURCE_FILES "Graph/*.cpp")

# Combine all datablock analysis source files
list(APPEND CARTS_DATABLOCK_ANALYSIS_SOURCE_FILES ${CARTS_DATABLOCK_GRAPH_SOURCE_FILES})

# Set the source files for parent scope
set(CARTS_DATABLOCK_ANALYSIS_SOURCE_FILES ${CARTS_DATABLOCK_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)
