CPMAddPackage(
        NAME immer
        GIT_TAG v0.8.1
        GITHUB_REPOSITORY arximboldi/immer
        OPTIONS
        "immer_BUILD_TESTS OFF"
        "immer_BUILD_EXAMPLES OFF"
        "immer_BUILD_DOCS OFF"
        "immer_BUILD_EXTRAS OFF")

CPMAddPackage(
        NAME yaml-cpp
        GITHUB_REPOSITORY jbeder/yaml-cpp
        GIT_TAG master
        OPTIONS "YAML_CPP_BUILD_TESTS OFF"
)

add_library(imagiro_processor STATIC)

target_sources(imagiro_processor PRIVATE
        "src/Processor.cpp"
        "src/ProcessorBase.cpp"
        "src/preset/Preset.cpp"
        "src/parameter/Parameter.cpp"
        "src/parameter/ParameterHelpers.cpp"
        "src/parameter/ParameterLoader.cpp"
        "src/config/AuthorizationManager.cpp"
        "src/preset/FileBackedPreset.cpp"
        "src/dsp/filter/StateVariableTPTFilter.cpp"
        "src/envelope/adsr/ADSR.cpp"
        "src/parameter/modulation/ModMatrix.cpp"
        "src/grain/Grain.cpp"
        "src/BufferFileLoader.cpp"
        "src/dsp/transient/TransientDetector.cpp"
        "src/bufferpool/BufferLoader.cpp"
        "src/bufferpool/BufferRequest.cpp"
        "src/bufferpool/BufferRequestHandle.cpp"
        "src/bufferpool/FileBufferCache.cpp"
)

target_include_directories(imagiro_processor PUBLIC
        src
        ${immer_SOURCE_DIR}
)

target_link_libraries(imagiro_processor PUBLIC
        juce::juce_audio_basics
        yaml-cpp
        choc
        imagiro_util
)