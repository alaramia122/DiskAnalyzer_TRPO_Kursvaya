cmake_minimum_required(VERSION 3.20)

project(DiskAnalyzer_TRPO_Kursovaya)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Charts
)

add_executable(DiskAnalyzer_TRPO_Kursovaya
    src/main.cpp
 "src/domain/enums/FileCategory.h" "src/domain/enums/ScanStatus.h" "src/domain/enums/ExportFormat.h" "src/domain/models/FileInfo.h" "src/domain/enums/FileSystemObjectType.h" "src/domain/models/DirectoryStatistics.h" "src/domain/models/DirectoryNode.h" "src/domain/models/AnalysisResult.h" "src/domain/models/DirectoryStatistics.cpp")

target_link_libraries(DiskAnalyzer_TRPO_Kursovaya PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Charts
)

target_include_directories(DiskAnalyzer_TRPO_Kursovaya PRIVATE
    src
)

if(WIN32)

    set(QT_ROOT
        "D:/Qt5.15.14-Windows-x86_64-VS2017-15.9.62-20240527/Qt5.15.14-Windows-x86_64-VS2017-15.9.62"
    )

    set(QT_BIN_DIR "${QT_ROOT}/bin")
    set(QT_PLUGINS_DIR "${QT_ROOT}/plugins")

    add_custom_command(TARGET  DiskAnalyzer_TRPO_Kursovaya  POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_BIN_DIR}/Qt5Core.dll"
            $<TARGET_FILE_DIR:DiskAnalyzer_TRPO_Kursovaya>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_BIN_DIR}/Qt5Gui.dll"
            $<TARGET_FILE_DIR:DiskAnalyzer_TRPO_Kursovaya>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_BIN_DIR}/Qt5Widgets.dll"
            $<TARGET_FILE_DIR:DiskAnalyzer_TRPO_Kursovaya>

        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DiskAnalyzer_TRPO_Kursovaya>/platforms"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/platforms/qwindows.dll"
            "$<TARGET_FILE_DIR:DiskAnalyzer_TRPO_Kursovaya>/platforms"

        COMMENT "Copying Qt runtime dependencies..."
    )

endif()