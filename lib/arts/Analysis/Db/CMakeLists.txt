# Add source files for db analysis
file(GLOB CARTS_DB_SOURCE_FILES "*.cpp")

# Add the new data flow analysis files
set(CARTS_DB_SOURCE_FILES
    ${CARTS_DB_SOURCE_FILES}
    DbInfo.cpp
    DbDataFlowAnalysis.cpp
)

# Add graph subdirectory
add_subdirectory(Graph)

# Set the source files for parent scope
set(CARTS_DB_SOURCE_FILES ${CARTS_DB_SOURCE_FILES} PARENT_SCOPE)
