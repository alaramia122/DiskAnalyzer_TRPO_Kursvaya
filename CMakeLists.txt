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

    src/domain/enums/FileCategory.h
    src/domain/enums/ScanStatus.h
    src/domain/enums/ExportFormat.h
    src/domain/enums/FileSystemObjectType.h

    src/domain/interfaces/IFileClassifier.h

    src/domain/models/FileInfo.h
    src/domain/models/DirectoryStatistics.h
    src/domain/models/DirectoryStatistics.cpp
    src/domain/models/DirectoryNode.h
    src/domain/models/DirectoryNode.cpp
    src/domain/models/AnalysisResult.h
    src/domain/models/AnalysisResult.cpp

    src/infrastructure/classifier/FileClassifier.h
    src/infrastructure/classifier/FileClassifier.cpp
)

target_link_libraries(DiskAnalyzer_TRPO_Kursovaya PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Charts
)

target_include_directories(DiskAnalyzer_TRPO_Kursovaya PRIVATE
    src
)

if(WIN32)

    get_target_property(QMAKE_EXE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE
        windeployqt
        HINTS "${QT_BIN_DIR}"
    )

    add_custom_command(TARGET DiskAnalyzer_TRPO_Kursovaya POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:DiskAnalyzer_TRPO_Kursovaya>"
        COMMENT "Deploying Qt runtime..."
    )

endif()