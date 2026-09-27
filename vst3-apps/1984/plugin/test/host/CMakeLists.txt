#  A real VST3 host, small enough to read: it loads a 1984 bundle the way a
#  DAW does and prints what the DAW would be offered.
#
#    cmake -S test/host -B test/host/build -A x64
#    cmake --build test/host/build --config Release
#    test/host/build/n84host_artefacts/Release/n84host.exe [path.vst3]
cmake_minimum_required(VERSION 3.22)
project(N84Host VERSION 1.0.0)
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
juce_add_console_app(n84host PRODUCT_NAME "n84host")
target_sources(n84host PRIVATE host.cpp)
target_compile_definitions(n84host PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(n84host PRIVATE
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_audio_devices juce::juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
