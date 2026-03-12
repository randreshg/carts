file(GLOB CARTS_TRANSFORMS_SOURCE_FILES CONFIGURE_DEPENDS
    "*.cpp"
    "db/*.cpp"
    "dep/*.cpp"
    "db/Block/*.cpp"
    "db/ElementWise/*.cpp"
    "db/Stencil/*.cpp"
    "db/Versioned/*.cpp"
    "edt/*.cpp"
    "edt/tiling2d/*.cpp"
    "loop/*.cpp"
)

set(CARTS_TRANSFORMS_SOURCE_FILES ${CARTS_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
