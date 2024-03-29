# Sources
set(Srcs 
  Pass.cpp
  PDGAnalysis.cpp
  PDGAnalysis_metadata.cpp
  PDGAnalysis_metadata_embedder.cpp
  PDGAnalysis_metadata_scc_embedder.cpp
  PDGAnalysis_controlDependences.cpp
  PDGAnalysis_compare.cpp
  PDGAnalysis_memory.cpp
  PDGAnalysis_callGraph.cpp
  PDGAnalysis_library.cpp
  AnalysisPass.cpp
  IntegrationWithSVF.cpp
)

# Compilation flags
set_source_files_properties(${Srcs} PROPERTIES COMPILE_FLAGS " -std=c++17 -fPIC")

# Name of the LLVM pass
set(PassName "PDGAnalysis")

# configure LLVM 
find_package(LLVM 9 REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

message(STATUS "LLVM_DIR IS ${LLVM_CMAKE_DIR}.")

include_directories(${LLVM_INCLUDE_DIRS}
  ../include
  ../../basic_utilities/include
  ../../alloc_aa/include 
  ../../talkdown/include 
  ../../dataflow/include
  ../../call_graph/include
  ../../dg/include
  ../../pdg/include
  ../../loop_structure/include
  ../../alias_analysis_engine/include
  ../../dependence_analysis/include
  ../../call_graph_analysis/include
  ${CMAKE_INSTALL_PREFIX}/include
  ${CMAKE_INSTALL_PREFIX}/include/svf
  )

# Declare the LLVM pass to compile
add_llvm_library(${PassName} MODULE ${Srcs})
