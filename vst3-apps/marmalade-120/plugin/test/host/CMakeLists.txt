#  A real VST3 host, small enough to read: it loads a built Marmalade 120
#  bundle the way a DAW does, prints the parameters a DAW would be offered,
#  and renders audio through the wrapper - the one link the bench (engine)
#  and spicecheck (solver) cannot reach.
#
#    cmake -S test/host -B test/host/build -DJUCE_DIR=<your JUCE checkout>
#    cmake --build test/host/build --config Release
#    ./test/host/build/mmhost_artefacts/Release/mmhost "<path>/Marmalade 120.vst3"
cmake_minimum_required(VERSION 3.22)
project(MMHost VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "pass -DJUCE_DIR=<JUCE checkout>")
endif()

# These must reach the JUCE module sources, or the VST3 hosting classes do
# not exist at all. Set before the modules are added.
add_compile_definitions(
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    _CRT_SECURE_NO_WARNINGS)

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

juce_add_console_app(mmhost PRODUCT_NAME "mmhost")
target_sources(mmhost PRIVATE host.cpp)
target_compile_definitions(mmhost PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(mmhost PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)
