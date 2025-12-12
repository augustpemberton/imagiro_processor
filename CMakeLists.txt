CPMAddPackage(
        NAME immer
        GIT_TAG v0.9.0
        GITHUB_REPOSITORY arximboldi/immer
        SYSTEM
        OPTIONS
        "immer_BUILD_TESTS OFF"
        "immer_BUILD_EXAMPLES OFF"
        "immer_BUILD_DOCS OFF"
        "immer_BUILD_EXTRAS OFF")

CPMAddPackage("gh:nlohmann/json@3.12.0")

add_library(imagiro_processor STATIC)

# important: copy compile definitions from the main project
target_compile_definitions(imagiro_processor PUBLIC
        $<TARGET_PROPERTY:${ProjectName},COMPILE_DEFINITIONS>
)

target_sources(imagiro_processor PRIVATE
        "include/imagiro_processor/config/AuthorizationManager.cpp"
        "include/imagiro_processor/dsp/filter/StateVariableTPTFilter.cpp"
        "include/imagiro_processor/envelope/adsr/ADSR.cpp"
        "include/imagiro_processor/grain/Grain.cpp"
        "include/imagiro_processor/BufferFileLoader.cpp"
        "include/imagiro_processor/dsp/transient/TransientDetector.cpp"
        "include/imagiro_processor/bufferpool/BufferLoader.cpp"
        "include/imagiro_processor/bufferpool/BufferRequest.cpp"
        "include/imagiro_processor/bufferpool/BufferRequestHandle.cpp"
        "include/imagiro_processor/bufferpool/FileBufferCache.cpp"
)

target_include_directories(imagiro_processor PUBLIC
        include
)

target_link_libraries(imagiro_processor PUBLIC
        juce::juce_audio_basics
        choc
        imagiro_util
        immer
        nlohmann_json::nlohmann_json
)