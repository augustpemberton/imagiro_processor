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

# AUDIOFFT_APPLE_ACCELERATE is the ONE sanctioned platform-parallel backend
# (owner-approved exception to the no-parallel-implementations policy): AudioFFT's
# hot analysis FFTs run on Apple's vDSP here, and on the Ooura reference backend
# everywhere else. Ooura is the Linux-CI-tested reference path; keep it building.
# Attaching both the macro and the framework to the core INTERFACE means every
# plugin that links imagiro_processor_core inherits them (previously set per-plugin).
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

    # STATIC (not INTERFACE) so it compiles the one TU that instantiates
    # clap-helpers' Plugin/HostProxy out-of-line methods — plugins no longer
    # include plugin.hxx/host-proxy.hxx themselves.
    add_library(imagiro_processor_clap STATIC
            include/imagiro_processor/clap/ClapProcessor.cpp)
    target_link_libraries(imagiro_processor_clap PUBLIC
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

        # Shared, ivl-based device settings backing: implements
        # ivl::IDeviceSettings over clap-wrapper's StandaloneHost and registers
        # itself so any plugin's in-window settings panel works in standalone.
        # Compiled straight into the standalone executable so it sees the
        # standalone-only getStandaloneHost()/RtAudio/RtMidi symbols (inherited
        # from the wrapper lib) and the ivl headers (inherited from IMPL_TARGET).
        # Guarded on ivl so plugins that don't use ivl still build.
        if(TARGET ivl)
            target_sources(${_t} PRIVATE
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include/imagiro_processor/standalone/DeviceSettingsStandalone.cpp")
        endif()

        if(DEFINED ICW_ASSET_OUTPUT_DIRECTORY)
            set_target_properties(${_t} PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY "${ICW_ASSET_OUTPUT_DIRECTORY}")
        endif()
    endif()
endfunction()

# imagiro_add_clap_plugin(NAME <name>
#     CORE_SOURCES <dsp .cpp...>  [CORE_LIBS <extra libs...>] [CORE_PCH <pch.hpp>]
#     [INCLUDE_DIRS <dir...>]     [UI_SOURCES <editor .cpp...>] [GUI_SOURCES <gui-only .cpp...>]
#     CLAP_SOURCE <ClapProcessor-subclass .cpp>  ENTRY_SOURCE <clap_entry .cpp>
#     [BUILD_CLAP <bool>] [BUILD_WRAPPERS <bool>]
#     OUTPUT_NAME <name> CLAP_ID <id> BUNDLE_IDENTIFIER <id> BUNDLE_VERSION <v>
#     [VERSION_SCRIPT <file>] [FORMATS VST3 AUV2 AUV3 AAX STANDALONE]
#     [AUV2_MANUFACTURER_NAME/CODE AUV2_SUBTYPE_CODE AUV2_INSTRUMENT_TYPE]
#     [ASSET_OUTPUT_DIRECTORY <dir>])
#
# Hoists the whole clap-first plugin skeleton so a plugin's CMakeLists is one
# call plus its own source lists. Builds:
#   ${NAME}_core        STATIC — framework-free DSP core (+imagiro_fastmath, PCH)
#   ${NAME}_ui          STATIC — ivl editor tree (only when UI_SOURCES + TARGET ivl)
#   ${NAME}_clap_core   STATIC — ClapProcessor subclass + factory; +GUI when UI present
#   ${NAME}_clap        MODULE — .clap exporting clap_entry (default version script
#                                 generated if VERSION_SCRIPT is omitted)
#   ${NAME}_wrapped_*          — clap-wrapper formats via imagiro_add_clap_wrapper
# The core/ui are always built (so headless/wasm/test configs still get them);
# BUILD_CLAP / BUILD_WRAPPERS (default ON) gate the shell and the wrapper formats.
function(imagiro_add_clap_plugin)
    set(oneValueArgs
            NAME CORE_PCH CLAP_SOURCE ENTRY_SOURCE BUILD_CLAP BUILD_WRAPPERS
            OUTPUT_NAME CLAP_ID BUNDLE_IDENTIFIER BUNDLE_VERSION VERSION_SCRIPT
            ASSET_OUTPUT_DIRECTORY AUV2_MANUFACTURER_NAME AUV2_MANUFACTURER_CODE
            AUV2_SUBTYPE_CODE AUV2_INSTRUMENT_TYPE)
    set(multiValueArgs CORE_SOURCES CORE_LIBS INCLUDE_DIRS UI_SOURCES GUI_SOURCES FORMATS)
    cmake_parse_arguments(ICP "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DEFINED ICP_BUILD_CLAP)
        set(ICP_BUILD_CLAP ON)
    endif()
    if(NOT DEFINED ICP_BUILD_WRAPPERS)
        set(ICP_BUILD_WRAPPERS ON)
    endif()
    if(NOT DEFINED ICP_OUTPUT_NAME)
        set(ICP_OUTPUT_NAME "${ICP_NAME}")
    endif()

    set(_core ${ICP_NAME}_core)
    set(_ui ${ICP_NAME}_ui)
    set(_clap_core ${ICP_NAME}_clap_core)
    set(_clap ${ICP_NAME}_clap)

    # DSP core: links no external audio-framework target, so a stray framework
    # include in a core TU fails to compile.
    add_library(${_core} STATIC ${ICP_CORE_SOURCES})
    if(ICP_INCLUDE_DIRS)
        target_include_directories(${_core} PUBLIC ${ICP_INCLUDE_DIRS})
    endif()
    target_link_libraries(${_core} PUBLIC
            imagiro_processor_core imagiro_util_core ${ICP_CORE_LIBS})
    if(TARGET imagiro_fastmath)
        target_link_libraries(${_core} PRIVATE imagiro_fastmath)
    endif()
    if(ICP_CORE_PCH)
        target_precompile_headers(${_core} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:${ICP_CORE_PCH}>")
    endif()

    # Editor UI tree (built only where the ivl stack is configured; wasm skips it).
    # ivl's Label.h pulls a visage-generated embedded-fonts header by a literal
    # build-relative path, so consumers need the build root on their include path.
    set(_have_ui FALSE)
    if(ICP_UI_SOURCES AND TARGET ivl)
        add_library(${_ui} STATIC ${ICP_UI_SOURCES})
        target_link_libraries(${_ui} PUBLIC ${_core} ivl)
        target_include_directories(${_ui} PUBLIC "${CMAKE_BINARY_DIR}")
        set(_have_ui TRUE)
    endif()

    if(NOT ICP_BUILD_CLAP)
        return()
    endif()

    # Clap-first core: STATIC so the .clap module AND every wrapper format link the
    # exact same self-contained core. It exports the entry hooks but NOT clap_entry;
    # the ENTRY_SOURCE shim exports that per-format. GUI sources + the IMAGIRO_CLAP_GUI
    # define are PUBLIC so wrapped formats inherit the gui, while headless test
    # binaries that compile CLAP_SOURCE without linking this core stay gui-less.
    add_library(${_clap_core} STATIC ${ICP_CLAP_SOURCE})
    target_link_libraries(${_clap_core} PUBLIC ${_core} imagiro_processor_clap)
    if(_have_ui)
        if(ICP_GUI_SOURCES)
            target_sources(${_clap_core} PRIVATE ${ICP_GUI_SOURCES})
        endif()
        target_link_libraries(${_clap_core} PUBLIC ${_ui})
        target_compile_definitions(${_clap_core} PUBLIC IMAGIRO_CLAP_GUI)
    endif()

    # Headless .clap: SHARED module with a .clap suffix, exporting only clap_entry.
    add_library(${_clap} MODULE ${ICP_ENTRY_SOURCE})
    target_link_libraries(${_clap} PRIVATE ${_clap_core})
    set_target_properties(${_clap} PROPERTIES
            PREFIX "" SUFFIX ".clap" OUTPUT_NAME "${ICP_OUTPUT_NAME}")
    if(APPLE)
        set_target_properties(${_clap} PROPERTIES
                LINK_FLAGS "-Wl,-exported_symbol,_clap_entry")
    elseif(UNIX)
        set(_version_script "${ICP_VERSION_SCRIPT}")
        if(NOT _version_script)
            set(_version_script "${CMAKE_CURRENT_BINARY_DIR}/${ICP_NAME}_clap.version")
            file(WRITE "${_version_script}" "{ global: clap_entry; local: *; };\n")
        endif()
        target_link_options(${_clap} PRIVATE
                "LINKER:--version-script=${_version_script}")
    endif()

    # Additive VST3/AUv2/AUv3/AAX/Standalone bundles from the same static core.
    if(ICP_BUILD_WRAPPERS AND ICP_FORMATS)
        imagiro_add_clap_wrapper(
                TARGET_NAME ${ICP_NAME}_wrapped
                IMPL_TARGET ${_clap_core}
                ENTRY_SOURCE ${ICP_ENTRY_SOURCE}
                OUTPUT_NAME "${ICP_OUTPUT_NAME}"
                CLAP_ID "${ICP_CLAP_ID}"
                BUNDLE_IDENTIFIER "${ICP_BUNDLE_IDENTIFIER}"
                BUNDLE_VERSION "${ICP_BUNDLE_VERSION}"
                FORMATS ${ICP_FORMATS}
                AUV2_MANUFACTURER_NAME "${ICP_AUV2_MANUFACTURER_NAME}"
                AUV2_MANUFACTURER_CODE "${ICP_AUV2_MANUFACTURER_CODE}"
                AUV2_SUBTYPE_CODE "${ICP_AUV2_SUBTYPE_CODE}"
                AUV2_INSTRUMENT_TYPE "${ICP_AUV2_INSTRUMENT_TYPE}"
                ASSET_OUTPUT_DIRECTORY "${ICP_ASSET_OUTPUT_DIRECTORY}")
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
if(TARGET imagiro_fastmath)
    target_link_libraries(imagiro_processor PRIVATE imagiro_fastmath)
endif()

# Tests
option(IMAGIRO_PROCESSOR_BUILD_TESTS "Build imagiro_processor tests" ON)
if(IMAGIRO_PROCESSOR_BUILD_TESTS AND TARGET Catch2::Catch2WithMain)
    add_subdirectory(tests)
endif()