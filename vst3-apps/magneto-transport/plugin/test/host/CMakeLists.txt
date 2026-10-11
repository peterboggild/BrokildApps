#  A real VST3 host, small enough to read: it loads a Magneto Transport bundle the way a
#  DAW does and prints what the DAW would be offered.
#
#    cmake -S test/host -B test/host/build -A x64
#    cmake --build test/host/build --config Release
#    test/host/build/mthost_artefacts/Release/mthost.exe [path.vst3]
cmake_minimum_required(VERSION 3.22)
project(MTHost VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "C:/Users/peter/AudioDev/Projects/BrokildVSTTemplate/external/JUCE" CACHE PATH "JUCE checkout")
add_compile_definitions(
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    _CRT_SECURE_NO_WARNINGS)
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
juce_add_console_app(mthost PRODUCT_NAME "mthost")
target_sources(mthost PRIVATE host.cpp)
target_compile_definitions(mthost PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(mthost PRIVATE
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_audio_devices juce::juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
