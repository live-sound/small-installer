cmake_minimum_required(VERSION 3.23)
project(SmallInstaller LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Slim UI draws the wizard: the submodule, or a checkout the including project already has.
set(SI_SLIM_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/slim-ui" CACHE PATH "Slim UI checkout")
if(NOT EXISTS "${SI_SLIM_UI_DIR}/include/slim_ui/slim_ui.h")
    message(FATAL_ERROR "No Slim UI at ${SI_SLIM_UI_DIR}: run `git submodule update --init` or set SI_SLIM_UI_DIR")
endif()

# LZMA SDK (always included)
add_library(si_lzma STATIC
    third_party/lzma/LzmaDec.c
    third_party/lzma/LzmaEnc.c
    third_party/lzma/LzFind.c
    third_party/lzma/LzFindMt.c
    third_party/lzma/LzFindOpt.c
    third_party/lzma/CpuArch.c
    third_party/lzma/Threads.c
)
target_include_directories(si_lzma PUBLIC third_party/lzma)
# Favor a smaller decoder in distribution builds, at some decoding-speed cost.
target_compile_definitions(si_lzma PRIVATE $<$<CONFIG:MinSizeRel>:Z7_LZMA_SIZE_OPT>)
# Keep compiler stack cookies in the no-CRT stubs; final executables link the
# small static vcruntime implementation and initialize its cookie explicitly.
target_compile_options(si_lzma PRIVATE
    "$<$<CONFIG:MinSizeRel>:/GS;/guard:cf;/Gy;/Gw>"
)

# si_pack tool (needs encoder + decoder)
add_executable(si_pack tools/si_pack.cpp)
target_include_directories(si_pack PRIVATE include third_party/lzma)
target_link_libraries(si_pack PRIVATE si_lzma)

# An installer or uninstaller stub: si_add_wizard(<target> <sources...>). MinSizeRel builds it without the C runtime.
function(si_add_wizard target)
    add_executable(${target} WIN32 ${ARGN})
    set_target_properties(${target} PROPERTIES LANGUAGE C C_STANDARD 11)
    set_property(TARGET ${target} PROPERTY LINK_FLAGS "/MANIFEST:NO")
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include
        ${SI_SLIM_UI_DIR}/include
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/third_party/lzma
    )
    target_compile_definitions(${target} PRIVATE $<$<CONFIG:MinSizeRel>:SI_NO_CRT>)
    target_compile_options(${target} PRIVATE
        "$<$<CONFIG:MinSizeRel>:/GS;/guard:cf;/Oi;/Gy;/Gw>"
    )
    target_link_libraries(${target} PRIVATE
        si_lzma
        msimg32 d2d1 dwrite dwmapi
        user32 gdi32 shell32 ole32 comctl32 shlwapi advapi32 winmm
        # SI_NO_CRT keeps the stub on its custom entry point. These static
        # support libraries provide only the compiler-generated /GS, CFG, stack
        # probe, and exception-unwind helpers that the hardened build requires.
        $<$<CONFIG:MinSizeRel>:BufferOverflowU>
        $<$<CONFIG:MinSizeRel>:libcmt>
    )
    target_link_options(${target} PRIVATE
        "$<$<CONFIG:MinSizeRel>:/NODEFAULTLIB;/ENTRY:WinMainCRTStartup;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO;/DYNAMICBASE;/HIGHENTROPYVA;/NXCOMPAT;/GUARD:CF;/CETCOMPAT;/PDBALTPATH:%_PDB%>"
    )
endfunction()

option(SI_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(SI_BUILD_EXAMPLES "Build the example" ${PROJECT_IS_TOP_LEVEL})

if(SI_BUILD_TESTS)
    enable_testing()
    add_executable(si_transaction_test tests/si_transaction_test.c)
    target_include_directories(si_transaction_test PRIVATE include)
    target_link_libraries(si_transaction_test PRIVATE shell32)
    add_test(NAME si_transaction COMMAND si_transaction_test)

    add_executable(si_uninstall_files_test tests/si_uninstall_files_test.c)
    target_include_directories(si_uninstall_files_test PRIVATE
        include
        ${SI_SLIM_UI_DIR}/include
        third_party/lzma
    )
    target_link_libraries(si_uninstall_files_test PRIVATE
        msimg32 d2d1 dwrite dwmapi
        user32 gdi32 shell32 ole32 comctl32 shlwapi advapi32 winmm
    )
    add_test(NAME si_uninstall_files COMMAND si_uninstall_files_test)

    add_executable(si_ui_test tests/si_ui_test.c)
    target_include_directories(si_ui_test PRIVATE include ${SI_SLIM_UI_DIR}/include third_party/lzma)
    target_link_libraries(si_ui_test PRIVATE si_lzma msimg32 d2d1 dwrite dwmapi
        user32 gdi32 shell32 ole32 oleaut32 comctl32 shlwapi advapi32 winmm)
    add_test(NAME si_ui COMMAND si_ui_test)

    si_add_wizard(si_silent_uninstaller tests/si_silent_uninstaller.c)
    add_executable(si_silent_uninstall_test tests/si_silent_uninstall_test.c)
    target_link_libraries(si_silent_uninstall_test PRIVATE user32)
    target_compile_definitions(si_silent_uninstall_test PRIVATE SI_TEST_UNINSTALLER=L"$<TARGET_FILE:si_silent_uninstaller>")
    add_dependencies(si_silent_uninstall_test si_silent_uninstaller)
    add_test(NAME si_silent_uninstall COMMAND si_silent_uninstall_test)

    si_add_wizard(si_silent_installer tests/si_silent_installer.c)
    add_executable(si_silent_launch_test tests/si_silent_launch_test.c)
    target_link_libraries(si_silent_launch_test PRIVATE advapi32)
    target_compile_definitions(si_silent_launch_test PRIVATE SI_TEST_INSTALLER=L"$<TARGET_FILE:si_silent_installer>")
    add_dependencies(si_silent_launch_test si_silent_installer)
    add_test(NAME si_silent_launch COMMAND si_silent_launch_test)
endif()

if(SI_BUILD_EXAMPLES)
    add_subdirectory(examples/sample-app)
endif()
