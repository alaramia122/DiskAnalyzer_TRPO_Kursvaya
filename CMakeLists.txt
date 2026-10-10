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
    src/application/AnalysisPipeline.h
    src/application/AnalysisPipeline.cpp
    src/application/ApplicationController.h
    src/application/ApplicationController.cpp
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp

    src/domain/enums/FileCategory.h
    src/domain/enums/ScanStatus.h
    src/domain/enums/ExportFormat.h
    src/domain/enums/FileSystemObjectType.h

    src/domain/interfaces/IFileClassifier.h
    src/domain/interfaces/IDirectoryScanner.h
    src/domain/interfaces/IDirectoryAggregator.h

    src/domain/models/FileInfo.h
    src/domain/models/DirectoryStatistics.h
    src/domain/models/DirectoryStatistics.cpp
    src/domain/models/DirectoryNode.h
    src/domain/models/DirectoryNode.cpp
    src/domain/models/AnalysisResult.h
    src/domain/models/AnalysisResult.cpp

    src/infrastructure/classifier/FileClassifier.h
    src/infrastructure/classifier/FileClassifier.cpp

    src/infrastructure/scanner/DirectoryScanner.h
    src/infrastructure/scanner/DirectoryScanner.cpp

    src/infrastructure/aggregator/DirectoryAggregator.h
    src/infrastructure/aggregator/DirectoryAggregator.cpp
    src/infrastructure/export/ExportManager.h
    src/infrastructure/export/ExportManager.cpp
    src/visualization/models/VisualizationModel.h
    src/visualization/models/VisualizationModel.cpp
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

include(CTest)

if(BUILD_TESTING)
    add_executable(AnalysisPipelineIntegrationTest
        tests/AnalysisPipelineIntegrationTest.cpp

        src/domain/models/DirectoryStatistics.cpp
        src/domain/models/DirectoryNode.cpp
        src/domain/models/AnalysisResult.cpp

        src/infrastructure/classifier/FileClassifier.cpp
        src/infrastructure/scanner/DirectoryScanner.cpp
        src/infrastructure/aggregator/DirectoryAggregator.cpp
        src/application/AnalysisPipeline.cpp
    )

    target_include_directories(AnalysisPipelineIntegrationTest PRIVATE
        src
    )

    add_test(
        NAME AnalysisPipelineIntegrationTest
        COMMAND AnalysisPipelineIntegrationTest
    )

    add_executable(ExportManagerTest
        tests/ExportManagerTest.cpp

        src/domain/models/DirectoryStatistics.cpp
        src/domain/models/DirectoryNode.cpp
        src/domain/models/AnalysisResult.cpp

        src/infrastructure/classifier/FileClassifier.cpp
        src/infrastructure/scanner/DirectoryScanner.cpp
        src/infrastructure/aggregator/DirectoryAggregator.cpp
        src/infrastructure/export/ExportManager.cpp
        src/application/AnalysisPipeline.cpp
    )

    target_include_directories(ExportManagerTest PRIVATE
        src
    )

    add_test(
        NAME ExportManagerTest
        COMMAND ExportManagerTest
    )

    add_executable(ApplicationControllerTest
        tests/ApplicationControllerTest.cpp

        src/application/ApplicationController.h
        src/application/ApplicationController.cpp
        src/application/AnalysisPipeline.cpp

        src/domain/models/DirectoryStatistics.cpp
        src/domain/models/DirectoryNode.cpp
        src/domain/models/AnalysisResult.cpp

        src/infrastructure/classifier/FileClassifier.cpp
        src/infrastructure/scanner/DirectoryScanner.cpp
        src/infrastructure/aggregator/DirectoryAggregator.cpp
    )

    target_include_directories(ApplicationControllerTest PRIVATE
        src
    )

    target_link_libraries(ApplicationControllerTest PRIVATE
        Qt5::Core
    )

    add_test(
        NAME ApplicationControllerTest
        COMMAND ApplicationControllerTest
    )

    add_executable(VisualizationModelTest
        tests/VisualizationModelTest.cpp
        src/domain/models/DirectoryStatistics.cpp
        src/visualization/models/VisualizationModel.cpp
    )

    target_include_directories(VisualizationModelTest PRIVATE
        src
    )

    add_test(
        NAME VisualizationModelTest
        COMMAND VisualizationModelTest
    )
endif()
