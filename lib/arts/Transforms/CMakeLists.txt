file(GLOB CARTS_TRANSFORMS_SOURCE_FILES
    "*.cpp"
    "Datablock/*.cpp"
    "Datablock/Block/*.cpp"
    "Datablock/ElementWise/*.cpp"
    "Datablock/Stencil/*.cpp"
    "Datablock/Versioned/*.cpp"
    "Edt/*.cpp"
    "Loop/*.cpp"
)

set(CARTS_TRANSFORMS_SOURCE_FILES ${CARTS_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)