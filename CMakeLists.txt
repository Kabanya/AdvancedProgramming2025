cmake_minimum_required(VERSION 3.16)
project(AdvancedProgramming)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

# --- Скачиваем SDL3 ---
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.22
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rd_party/SDL3
)
FetchContent_MakeAvailable(SDL3)

# --- Собираем все .cpp из папки source ---
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/source/*.cpp
)

# --- Создаём исполняемый файл ---
add_executable(${PROJECT_NAME} ${SOURCES})

# --- Подключаем include директории ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/3rd_party
)

# --- Линкуем SDL3 ---
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

# --- Копируем assets ---
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# --- Добавляем Optick профайлер ---
set(OPTICK_INSTALL_TARGETS OFF CACHE BOOL "Don't install Optick" FORCE)
add_subdirectory(3rd_party/optick)

target_link_libraries(${PROJECT_NAME} PRIVATE OptickCore)
target_include_directories(${PROJECT_NAME} PRIVATE 3rd_party/optick/src)

# --- Копируем DLL файлы SDL3 в директорию сборки ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/
    COMMENT "Copying SDL3.dll to output directory"
)
