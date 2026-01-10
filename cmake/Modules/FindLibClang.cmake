if(TARGET llvm::libclang)
    return()
endif()

set(LIBCLANG_FOUND FALSE)

if(NOT LIBCLANG_LIBRARY)
    find_library(LIBCLANG_LIBRARY
        NAMES clang
        PATHS
        $ENV{LLVM_DIR}/lib
        "C:/Program Files/LLVM/lib"
        "/usr/lib/llvm-19/lib"
    )
endif()

if(NOT LIBCLANG_INCLUDE_DIR)
    find_path(LIBCLANG_INCLUDE_DIR
        NAMES clang-c/Index.h
        PATHS
        $ENV{LLVM_DIR}/include
        "C:/Program Files/LLVM/include"
        "/usr/lib/llvm-19/include"
    )
endif()

if(NOT LIBCLANG_LIBRARY OR NOT LIBCLANG_INCLUDE_DIR)
    return()
endif()

if(WIN32)
    find_file(LIBCLANG_BIN
        NAMES "libclang.dll"
        PATHS
        $ENV{LLVM_DIR}/bin
        "C:/Program Files/LLVM/bin"
    )

    if(LIBCLANG_BIN)
        add_library(llvm::libclang SHARED IMPORTED)

        set_target_properties(llvm::libclang PROPERTIES
            IMPORTED_LOCATION ${LIBCLANG_BIN}
            IMPORTED_IMPLIB ${LIBCLANG_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBCLANG_INCLUDE_DIR})

        set(LIBCLANG_FOUND TRUE)
    endif()
else()
    add_library(llvm::libclang SHARED IMPORTED)

    set_target_properties(llvm::libclang PROPERTIES
        IMPORTED_LOCATION ${LIBCLANG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${LIBCLANG_INCLUDE_DIR})

    set(LIBCLANG_FOUND TRUE)
endif()
