# Add all graph source files
file(GLOB CARTS_DB_ANALYSIS_GRAPH_SOURCE_FILES "*.cpp")

# Set the source files for parent scope
set(CARTS_DB_ANALYSIS_GRAPH_SOURCE_FILES ${CARTS_DB_ANALYSIS_GRAPH_SOURCE_FILES} PARENT_SCOPE)
