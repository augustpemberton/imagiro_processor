# imagiro_processor Test Suite

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

set(PROCESSOR_TEST_SOURCES
    PresetTests.cpp
    ParamRangeTests.cpp
    ValueFormatterTests.cpp
    ParamControllerTests.cpp
    ProcessStateTests.cpp
)

add_executable(imagiro_processor_tests ${PROCESSOR_TEST_SOURCES})

target_link_libraries(imagiro_processor_tests PRIVATE
    imagiro_processor
    imagiro_util
    juce::juce_audio_utils
    Melatonin::Perfetto
    Catch2::Catch2WithMain
)

target_include_directories(imagiro_processor_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../imagiro_util/include
)

# Copy compile definitions from parent project if available
if(TARGET ${ProjectName})
    target_compile_definitions(imagiro_processor_tests PRIVATE
        $<TARGET_PROPERTY:${ProjectName},COMPILE_DEFINITIONS>
    )
endif()

catch_discover_tests(imagiro_processor_tests)
