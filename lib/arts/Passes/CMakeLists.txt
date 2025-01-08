# Print it here
message(STATUS "POLYGEIST_INCLUDE_DIR: ${POLYGEIST_BUILD_DIR}/include")
message(STATUS "POLYGEIST_LIBRARY_DIR: ${POLYGEIST_BUILD_DIR}/lib")

include_directories(
  ${POLYGEIST_BUILD_DIR}/include
  ${POLYGEIST_DIR}/include
)

add_mlir_dialect_library(MLIRArtsTransforms
  ConvertOpenMPToARTS.cpp
  ConvertARTSToFuncs.cpp
  ${CARTS_CODEGEN_SOURCE_FILES}

DEPENDS
  MLIRArtsOpsIncGen
  MLIRArtsPassIncGen

LINK_LIBS PUBLIC
  MLIRAffineDialect
  MLIRArithDialect
  MLIRAsyncDialect
  MLIRAffineUtils
  MLIRFuncDialect
  MLIRFuncTransforms
  MLIRGPUToGPURuntimeTransforms
  MLIRGPUTransforms
  MLIRGPUToNVVMTransforms
  MLIRIR
  MLIRLLVMDialect
  MLIRMathDialect
  MLIRMathToLLVM
  MLIRMemRefDialect
  MLIRNVVMDialect
  MLIRPass
  MLIRArts
  MLIRSideEffectInterfaces
  MLIRSCFToControlFlow
  MLIRTargetLLVMIRImport
  MLIRTransformUtils
  MLIRGPUToROCDLTransforms
  MLIRControlFlowToLLVM
  MLIRMemRefToLLVM
  MLIRFuncToLLVM
  MLIRArithToLLVM
  MLIROpenMPToLLVM
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(MLIRArtsTransforms PUBLIC stdc++fs)
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
    target_link_libraries(MLIRArtsTransforms PUBLIC c++experimental)
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(MLIRArtsTransforms PUBLIC c++fs)
  endif()
endif()

# Find Polygeist lib
find_library(MLIR_POLYGEIST_LIB NAMES MLIRPolygeist PATHS ${POLYGEIST_BUILD_DIR}/lib)

if(MLIR_POLYGEIST_LIB)
  message(STATUS "Found Polygeist library: ${MLIR_POLYGEIST_LIB}")
  target_link_libraries(MLIRArtsTransforms PRIVATE ${MLIR_POLYGEIST_LIB})
else()
  message(FATAL_ERROR "Polygeist library not found")
endif()


target_compile_definitions(obj.MLIRArtsTransforms
  PRIVATE
  CARTS_PGO_DEFAULT_DATA_DIR="${CARTS_PGO_DEFAULT_DATA_DIR}"
  CARTS_PGO_ALTERNATIVE_ENV_VAR="${CARTS_PGO_ALTERNATIVE_ENV_VAR}"
  CARTS_PGO_DATA_DIR_ENV_VAR="${CARTS_PGO_DATA_DIR_ENV_VAR}"
)

