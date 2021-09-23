# Sources
set(Srcs
  LoopTransformer.cpp
  Pass.cpp
)

# Compilation flags
set_source_files_properties(${Srcs} PROPERTIES COMPILE_FLAGS " -std=c++17 -fPIC")

# Name of the LLVM pass
set(PassName "LoopTransformer")

# configure LLVM
find_package(LLVM REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

message(STATUS "LLVM_DIR IS ${LLVM_CMAKE_DIR}.")

include_directories(${LLVM_INCLUDE_DIRS}
           ../../basic_utilities/include
           ../../transformations/include
           ../../loop_structure/include 
           ../../pdg/include
           ../../invariants/include
           ../../induction_variables/include
           ../../loops/include
					 ../include
                     )

# Declare the LLVM pass to compile
add_llvm_library(${PassName} MODULE ${Srcs})