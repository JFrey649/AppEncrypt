cmake_minimum_required(VERSION 3.16)

project(AppEncrypt VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# --- SQLite (bundled amalgamation) ---
set(SQLITE3_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/sqlite")
if(EXISTS "${SQLITE3_SOURCE_DIR}/sqlite3.c")
    add_library(sqlite3 STATIC "${SQLITE3_SOURCE_DIR}/sqlite3.c")
    set_target_properties(sqlite3 PROPERTIES LINKER_LANGUAGE C)
    set_source_files_properties("${SQLITE3_SOURCE_DIR}/sqlite3.c" PROPERTIES LANGUAGE C)
    target_include_directories(sqlite3 PUBLIC "${SQLITE3_SOURCE_DIR}")
    target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1)
    set(SQLITE3_TARGET sqlite3)
else()
    message(WARNING "sqlite3.c not found - run scripts/fetch_sqlite.bat")
    add_library(sqlite3 INTERFACE)
    target_include_directories(sqlite3 INTERFACE "${SQLITE3_SOURCE_DIR}")
    set(SQLITE3_TARGET sqlite3)
endif()

# --- Core library ---
add_library(appencrypt_core STATIC
    src/core/AppConfig.cpp
    src/core/CryptoService.cpp
    src/core/Database.cpp
    src/core/EncryptService.cpp
    src/core/FileLayout.cpp
    src/core/ProcessUtil.cpp
    src/core/ShellRegistration.cpp
    src/cli/CommandLineParser.cpp
)
target_include_directories(appencrypt_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(appencrypt_core PUBLIC ${SQLITE3_TARGET} bcrypt crypt32)

# --- Main application (Qt GUI + CLI) ---
set(APPENCRYPT_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/PasswordDialog.cpp
    src/ui/ProgressDialog.cpp
)
add_executable(AppEncrypt ${APPENCRYPT_SOURCES})
target_link_libraries(AppEncrypt PRIVATE
    appencrypt_core
    sqlite3
    Qt6::Core
    Qt6::Widgets
)

# --- Stub launcher (no Qt, minimal size) ---
add_subdirectory(stub)

# --- Install / packaging ---
install(TARGETS AppEncrypt AppEncryptStub RUNTIME DESTINATION .)
install(FILES README.md DESTINATION . OPTIONAL)

# Post-build: copy Stub next to main exe (便于本地测试与打包)
add_custom_command(TARGET AppEncrypt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:AppEncryptStub>"
        "$<TARGET_FILE_DIR:AppEncrypt>/AppEncryptStub.exe"
    COMMENT "Copy AppEncryptStub.exe beside AppEncrypt.exe"
)