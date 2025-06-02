# Graph subdirectory for datablock analysis
# This file ensures proper compilation of graph-related source files

# Add all graph source files
file(GLOB CARTS_DATABLOCK_GRAPH_SOURCE_FILES "*.cpp")

# Set the source files for parent scope
set(CARTS_DATABLOCK_GRAPH_SOURCE_FILES ${CARTS_DATABLOCK_GRAPH_SOURCE_FILES} PARENT_SCOPE) 