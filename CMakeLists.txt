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

# JUCE-free audio-file decoder (miniaudio). A small STATIC lib because it needs
# exactly one MA_IMPLEMENTATION TU; the rest (loader) is a thin wrapper over it.
# Standalone-linkable so any plugin/test can pull real file decoding without JUCE.
add_library(imagiro_audio_decode STATIC
        include/imagiro_processor/miniaudio/miniaudio_impl.cpp
        include/imagiro_processor/bufferpool/MiniaudioSourceLoader.cpp
)
target_include_directories(imagiro_audio_decode PUBLIC include)
target_link_libraries(imagiro_audio_decode PUBLIC imagiro_processor_core)
find_package(Threads QUIET)
if(TARGET Threads::Threads)
    target_link_libraries(imagiro_audio_decode PUBLIC Threads::Threads)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(imagiro_audio_decode PUBLIC ${CMAKE_DL_LIBS} m)
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
            imagiro_audio_decode
            clap
            clap-helpers)

    # clap-wrapper: turns a clap-first plugin into VST3/AUv2/AUv3/AAX/Standalone.
    # Generic to every imagiro plugin, so the fetch + helper live here. Pinned to
    # a released tag (never a moving branch). The wrapper reuses the `clap` target
    # already fetched above (guarantee_clap aliases it), and downloads the VST3 /
    # rtaudio / rtmidi SDKs at configure time via its own CPM.
    option(IMAGIRO_PROCESSOR_CLAP_WRAPPERS "Provide clap-wrapper format helpers" ON)
    if(IMAGIRO_PROCESSOR_CLAP_WRAPPERS AND NOT TARGET clap-wrapper-shared-detail)
        set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)
        set(CLAP_WRAPPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        CPMAddPackage(
                NAME clap-wrapper
                GITHUB_REPOSITORY free-audio/clap-wrapper
                GIT_TAG v0.15.0)
    endif()
endif()

# imagiro_add_clap_wrapper(TARGET_NAME <prefix> IMPL_TARGET <static-lib>
#                          ENTRY_SOURCE <clap_entry.cpp> OUTPUT_NAME <name>
#                          CLAP_ID <id> BUNDLE_IDENTIFIER <id> BUNDLE_VERSION <v>
#                          FORMATS VST3 AUV2 AUV3 AAX STANDALONE
#                          AUV2_MANUFACTURER_NAME/CODE AUV2_SUBTYPE_CODE
#                          AUV2_INSTRUMENT_TYPE ASSET_OUTPUT_DIRECTORY)
#
# Builds one self-contained bundle per requested format from a clap-first static
# IMPL_TARGET (which exports the clap factory hooks) plus the tiny ENTRY_SOURCE.
# The .clap module itself is intentionally NOT built here — plugins own that so
# they keep full control of its symbol export/version-script. Formats unbuildable
# on the host toolchain (AUv2/AUv3 off macOS, AUv3 without Xcode, AAX without the
# gated capability) are skipped silently so configure never breaks.
function(imagiro_add_clap_wrapper)
    set(oneValueArgs
            TARGET_NAME IMPL_TARGET ENTRY_SOURCE OUTPUT_NAME
            CLAP_ID BUNDLE_IDENTIFIER BUNDLE_VERSION ASSET_OUTPUT_DIRECTORY
            AUV2_MANUFACTURER_NAME AUV2_MANUFACTURER_CODE AUV2_SUBTYPE_CODE
            AUV2_INSTRUMENT_TYPE)
    set(multiValueArgs FORMATS)
    cmake_parse_arguments(ICW "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET clap-wrapper-shared-detail)
        message(STATUS "imagiro: clap-wrapper unavailable; skipping ${ICW_TARGET_NAME} formats")
        return()
    endif()
    if(NOT DEFINED ICW_OUTPUT_NAME)
        set(ICW_OUTPUT_NAME "${ICW_TARGET_NAME}")
    endif()

    list(FIND ICW_FORMATS VST3 _do_vst3)
    list(FIND ICW_FORMATS AUV2 _do_auv2)
    list(FIND ICW_FORMATS AUV3 _do_auv3)
    list(FIND ICW_FORMATS AAX  _do_aax)
    list(FIND ICW_FORMATS STANDALONE _do_sa)

    if(_do_vst3 GREATER -1)
        set(_t ${ICW_TARGET_NAME}_vst3)
        add_library(${_t} MODULE)
        target_sources(${_t} PRIVATE ${ICW_ENTRY_SOURCE})
        target_link_libraries(${_t} PRIVATE ${ICW_IMPL_TARGET})
        target_add_vst3_wrapper(TARGET ${_t}
                OUTPUT_NAME "${ICW_OUTPUT_NAME}"
                BUNDLE_IDENTIFIER "${ICW_BUNDLE_IDENTIFIER}.vst3"
                BUNDLE_VERSION "${ICW_BUNDLE_VERSION}"
                ASSET_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
    endif()

    if(APPLE AND _do_auv2 GREATER -1)
        set(_t ${ICW_TARGET_NAME}_auv2)
        add_library(${_t} MODULE)
        target_sources(${_t} PRIVATE ${ICW_ENTRY_SOURCE})
        target_link_libraries(${_t} PRIVATE ${ICW_IMPL_TARGET})
        target_add_auv2_wrapper(TARGET ${_t}
                OUTPUT_NAME "${ICW_OUTPUT_NAME}"
                BUNDLE_IDENTIFIER "${ICW_BUNDLE_IDENTIFIER}.auv2"
                BUNDLE_VERSION "${ICW_BUNDLE_VERSION}"
                MANUFACTURER_NAME "${ICW_AUV2_MANUFACTURER_NAME}"
                MANUFACTURER_CODE "${ICW_AUV2_MANUFACTURER_CODE}"
                SUBTYPE_CODE "${ICW_AUV2_SUBTYPE_CODE}"
                INSTRUMENT_TYPE "${ICW_AUV2_INSTRUMENT_TYPE}")
        if(DEFINED ICW_ASSET_OUTPUT_DIRECTORY)
            set_target_properties(${_t} PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
        endif()
    endif()

    if(APPLE AND _do_auv3 GREATER -1 AND CMAKE_GENERATOR STREQUAL "Xcode")
        set(_t ${ICW_TARGET_NAME}_auv3)
        add_executable(${_t})
        target_sources(${_t} PRIVATE ${ICW_ENTRY_SOURCE})
        target_link_libraries(${_t} PRIVATE ${ICW_IMPL_TARGET})
        target_compile_definitions(${_t} PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)
        target_add_auv3_wrapper(TARGET ${_t}
                OUTPUT_NAME "${ICW_OUTPUT_NAME}"
                BUNDLE_IDENTIFIER "${ICW_BUNDLE_IDENTIFIER}.auv3"
                BUNDLE_VERSION "${ICW_BUNDLE_VERSION}"
                MANUFACTURER_NAME "${ICW_AUV2_MANUFACTURER_NAME}"
                MANUFACTURER_CODE "${ICW_AUV2_MANUFACTURER_CODE}"
                SUBTYPE_CODE "${ICW_AUV2_SUBTYPE_CODE}"
                INSTRUMENT_TYPE "${ICW_AUV2_INSTRUMENT_TYPE}")
        if(DEFINED ICW_ASSET_OUTPUT_DIRECTORY)
            set_target_properties(${_t} PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
        endif()
    endif()

    if(CLAP_WRAPPER_CAN_BUILD_AAX AND _do_aax GREATER -1)
        set(_t ${ICW_TARGET_NAME}_aax)
        add_library(${_t} MODULE)
        target_sources(${_t} PRIVATE ${ICW_ENTRY_SOURCE})
        target_link_libraries(${_t} PRIVATE ${ICW_IMPL_TARGET})
        target_add_aax_wrapper(TARGET ${_t}
                OUTPUT_NAME "${ICW_OUTPUT_NAME}"
                BUNDLE_IDENTIFIER "${ICW_BUNDLE_IDENTIFIER}.aaxplugin"
                BUNDLE_VERSION "${ICW_BUNDLE_VERSION}"
                ASSET_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
    endif()

    if(_do_sa GREATER -1)
        set(_t ${ICW_TARGET_NAME}_standalone)
        add_executable(${_t})
        target_sources(${_t} PRIVATE ${ICW_ENTRY_SOURCE})
        target_link_libraries(${_t} PRIVATE ${ICW_IMPL_TARGET})
        target_add_standalone_wrapper(TARGET ${_t}
                OUTPUT_NAME "${ICW_OUTPUT_NAME}"
                BUNDLE_VERSION "${ICW_BUNDLE_VERSION}"
                STATICALLY_LINKED_CLAP_ENTRY TRUE
                PLUGIN_ID "${ICW_CLAP_ID}")
        if(DEFINED ICW_ASSET_OUTPUT_DIRECTORY)
            set_target_properties(${_t} PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
        endif()
    endif()
endfunction()

# JUCE-free static lib exposing the parameter/preset/processor/synth headers plus
# the small set of out-of-line sources (ADSR). Header-only for the rest.
add_library(imagiro_processor STATIC
        "include/imagiro_processor/envelope/adsr/ADSR.cpp"
)

target_include_directories(imagiro_processor PUBLIC
        include
)

target_link_libraries(imagiro_processor PUBLIC
        imagiro_processor_core
        imagiro_util
        immer
        nlohmann_json::nlohmann_json
)

# Tests
option(IMAGIRO_PROCESSOR_BUILD_TESTS "Build imagiro_processor tests" ON)
if(IMAGIRO_PROCESSOR_BUILD_TESTS AND TARGET Catch2::Catch2WithMain)
    add_subdirectory(tests)
endif()