file(GLOB CARTS_TRANSFORMS_SOURCE_FILES CONFIGURE_DEPENDS
    "*.cpp"
    "Db/*.cpp"
    "Dependence/*.cpp"
    "Db/Block/*.cpp"
    "Db/ElementWise/*.cpp"
    "Db/Stencil/*.cpp"
    "Db/Versioned/*.cpp"
    "Edt/*.cpp"
    "Edt/Block/*.cpp"
    "Edt/Tiling2D/*.cpp"
    "Loop/*.cpp"
)

set(CARTS_TRANSFORMS_SOURCE_FILES ${CARTS_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
