#  A real VST3 host, small enough to read: it loads the INSTALLED bundle and
#
#  prints the parameter list a DAW would be offered.
#  This drives the shipped .vst3 bundles the way a DAW does — two findings in
#  one process, message thread pumping so their timers run — because that is
#  the one link the other two cannot reach.
#
#    cmake -S test/host -B test/host/build
#    cmake --build test/host/build --config Release
#
#  JUCE_DIR defaults to the checkout the fleet builds against.
cmake_minimum_required(VERSION 3.22)
project(BOHost VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "C:/Users/peter/AudioDev/Projects/BrokildVSTTemplate/external/JUCE"
    CACHE PATH "JUCE checkout")

# These must reach the JUCE MODULE sources, not just host.cpp: without them
# AudioPluginFormatManager::addDefaultFormats is a deleted function and the
# VST3 classes do not exist at all. Set them before the modules are added.
add_compile_definitions(
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_MODAL_LOOPS_PERMITTED=1     # the host pumps the message thread itself
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    _CRT_SECURE_NO_WARNINGS)

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

juce_add_console_app(bohost PRODUCT_NAME "bohost")
target_sources(bohost PRIVATE host.cpp)

target_compile_definitions(bohost PRIVATE JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(bohost PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
