file(GLOB CARTS_SDE_TRANSFORMS_SOURCE_FILES CONFIGURE_DEPENDS
    "state/*.cpp"
    "state/raising/*.cpp"
    "state/codelet/*.cpp"
    "dep/loop/*.cpp"
    "dep/fusion/*.cpp"
    "effect/scheduling/*.cpp"
    "effect/distribution/*.cpp"
)
set(CARTS_SDE_TRANSFORMS_SOURCE_FILES ${CARTS_SDE_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
