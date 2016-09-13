cmake_minimum_required(VERSION 3.4)

# User must set LLVM_ROOT to a valid install root,
# either via command line, or below
set(LLVM_ROOT "path/to/your/llvm/directory" CACHE STRING "")
if (NOT EXISTS ${LLVM_ROOT}/include/llvm)
	message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is invalid")
endif()

list(APPEND CMAKE_PREFIX_PATH "${LLVM_ROOT}/cmake")

# User must set LLVM_DIR to where  LLVMConfig.cmake is located
# either via command line, or below
set(LLVM_DIR "path/to/dir/containing/LLVMConfig.cmake" CACHE STRING "")

find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
set(LLVM_RUNTIME_OUTPUT_INTDIR "${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INT_DIR}")
set(LLVM_LIBRARY_OUTPUT_INTDIR "${CMAKE_BINARY_DIR}/lib/${CMAKE_CFG_INT_DIR}")

add_subdirectory(MagentaInterruptContext)
