# Add the main analysis source files
file(GLOB CARTS_ANALYSIS_SOURCE_FILES "*.cpp")

# Add the Datablock subdirectory
add_subdirectory(Datablock)

# Add the EDT subdirectory
add_subdirectory(Edt)

# Combine all analysis source files
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_DATABLOCK_ANALYSIS_SOURCE_FILES})
list(APPEND CARTS_ANALYSIS_SOURCE_FILES ${CARTS_EDT_ANALYSIS_SOURCE_FILES})

set(CARTS_ANALYSIS_SOURCE_FILES ${CARTS_ANALYSIS_SOURCE_FILES} PARENT_SCOPE)