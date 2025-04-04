if(NOT LIBCLANG_LIBRARY)
    find_library(LIBCLANG_LIBRARY
        NAMES clang
        PATHS
        $ENV{LLVM_DIR}/lib
        "C:/Program Files/LLVM/lib"
        "/usr/lib"
        "/usr/local/lib"
    )
endif()

# Optionally, find the headers if you need to include them in your project
find_path(LIBCLANG_INCLUDE_DIR
    NAMES clang-c/Index.h
    PATHS
    $ENV{LLVM_DIR}/include
    "C:/Program Files/LLVM/include"
    "/usr/include"
    "/usr/local/include"
)

# Define search paths and library names for different platforms
if(WIN32)
    # Windows specific settings
    set(LIBCLANG_LIB_NAME "libclang.dll")
elseif(APPLE)
    # macOS specific settings (use libclang.dylib)
    set(LIBCLANG_LIB_NAME "libclang.dylib")
else()
    # Linux/Unix specific settings
    set(LIBCLANG_LIB_NAME "libclang.so")
endif()

find_file(LIBCLANG_BIN
    NAMES ${LIBCLANG_LIB_NAME}
    PATHS
    $ENV{LLVM_DIR}/bin
    "C:/Program Files/LLVM/bin"
    "/usr/bin"
    "/usr/local/bin"
)

# If the library is found, set the necessary variables
if(LIBCLANG_LIBRARY AND LIBCLANG_INCLUDE_DIR AND LIBCLANG_BIN)
    if(NOT TARGET llvm::libclang)
        add_library(llvm::libclang SHARED IMPORTED)

        set_target_properties(llvm::libclang PROPERTIES
            IMPORTED_LOCATION ${LIBCLANG_BIN}
            IMPORTED_IMPLIB ${LIBCLANG_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBCLANG_INCLUDE_DIR})
    endif()

    set(LIBCLANG_FOUND TRUE)
else()
    set(LIBCLANG_FOUND FALSE)
endif()