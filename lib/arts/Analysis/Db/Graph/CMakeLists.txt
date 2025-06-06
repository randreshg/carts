# Graph subdirectory for db analysis
# This file ensures proper compilation of graph-related source files

# Add all graph source files
file(GLOB CARTS_DB_GRAPH_SOURCE_FILES "*.cpp")

# Set the source files for parent scope
set(CARTS_DB_GRAPH_SOURCE_FILES ${CARTS_DB_GRAPH_SOURCE_FILES} PARENT_SCOPE) 