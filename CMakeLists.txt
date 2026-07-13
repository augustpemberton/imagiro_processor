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

add_library(imagiro_processor_core INTERFACE)

target_include_directories(imagiro_processor_core INTERFACE
        include
)

target_link_libraries(imagiro_processor_core INTERFACE
        imagiro_util_core
        immer
        nlohmann_json::nlohmann_json
)

# AudioFFT + downstream DSP use vDSP under AUDIOFFT_APPLE_ACCELERATE. Attaching
# both the macro and the framework to the core INTERFACE means every plugin that
# links imagiro_processor_core inherits them (previously set per-plugin).
if(APPLE)
    target_compile_definitions(imagiro_processor_core INTERFACE AUDIOFFT_APPLE_ACCELERATE)
    target_link_libraries(imagiro_processor_core INTERFACE "-framework Accelerate")
endif()

# Reusable JUCE-free headless CLAP shell (imagiro::ClapProcessor). Guarded so
# sibling plugins can opt out, but on by default. Fetches the header-only clap +
# clap-helpers here so any plugin inherits them by linking imagiro_processor_clap.
option(IMAGIRO_PROCESSOR_CLAP "Provide the reusable CLAP processor shell" ON)
if(IMAGIRO_PROCESSOR_CLAP)
    if(NOT TARGET clap)
        CPMAddPackage("gh:free-audio/clap#1.2.6")
    endif()
    if(NOT TARGET clap-helpers)
        # clap-helpers 'main' tracks clap's unreleased 'next' branch; pin to the
        # "CLAP 1.2.6" commit so the helpers match the clap headers above.
        CPMAddPackage("gh:free-audio/clap-helpers#58ab81b1dc8219e859529c1306f364bb3aedf7d5")
    endif()

    add_library(imagiro_processor_clap INTERFACE)
    target_link_libraries(imagiro_processor_clap INTERFACE
            imagiro_processor_core
            clap
            clap-helpers)
endif()

if(TARGET juce::juce_core)

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
        imagiro_processor_core
        juce::juce_audio_basics
        imagiro_util
        immer
        nlohmann_json::nlohmann_json
)

endif() # TARGET juce::juce_core

# Tests
option(IMAGIRO_PROCESSOR_BUILD_TESTS "Build imagiro_processor tests" ON)
if(IMAGIRO_PROCESSOR_BUILD_TESTS AND TARGET Catch2::Catch2WithMain)
    add_subdirectory(tests)
endif()