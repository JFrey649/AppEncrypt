add_executable(AppEncryptStub stub_main.cpp)
target_link_libraries(AppEncryptStub PRIVATE shell32)

if(WIN32)
    set_target_properties(AppEncryptStub PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "AppEncryptStub"
    )
endif()

install(TARGETS AppEncryptStub RUNTIME DESTINATION .)
