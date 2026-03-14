file(GLOB CARTS_TRANSFORMS_SOURCE_FILES CONFIGURE_DEPENDS
    "*.cpp"
    "db/*.cpp"
    "dep/*.cpp"
    "db/block/*.cpp"
    "db/elementwise/*.cpp"
    "db/stencil/*.cpp"
    "db/transforms/*.cpp"
    "edt/*.cpp"
    "edt/tiling2d/*.cpp"
    "kernel/*.cpp"
    "pattern/*.cpp"
    "loop/*.cpp"
)

set(CARTS_TRANSFORMS_SOURCE_FILES ${CARTS_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
