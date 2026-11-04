# The application the example installs, and its installer and uninstaller stubs. build.ps1 builds and packs them.
add_executable(SampleApp WIN32 app/sample_app.c app/sample_app.rc)
target_include_directories(SampleApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

si_add_wizard(SampleSetup installer.c installer.rc)
target_include_directories(SampleSetup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

si_add_wizard(SampleUninstall uninstaller.c uninstaller.rc)
target_include_directories(SampleUninstall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
