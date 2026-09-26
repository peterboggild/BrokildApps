# The drum family's shared machine-style parts, for Kickstart, Snare Tactics,
# Hats Off and Beetmachine.
#
#   include(<path>/machine-art/machine-art.cmake)
#   brokild_add_machine_art(<target>)
#
# Adds MachineArt.cpp, the include path for MachineArt.h, and the art itself as
# binary data (namespace MachineArtData, header MachineArtData.h). The art is
# globbed at CONFIGURE time: re-run configure after ingest.ps1 changes it.

set(MACHINE_ART_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(brokild_add_machine_art target)
    file(GLOB art "${MACHINE_ART_DIR}/art/*.png" "${MACHINE_ART_DIR}/art/*.jpg")
    list(LENGTH art n)
    if(n EQUAL 0)
        message(FATAL_ERROR "machine-art: no art in ${MACHINE_ART_DIR}/art - run ingest.ps1")
    endif()
    message(STATUS "${target}: ${n} machine-art parts embedded")
    juce_add_binary_data(${target}MachineArt HEADER_NAME MachineArtData.h NAMESPACE MachineArtData SOURCES ${art})
    target_link_libraries(${target} PRIVATE ${target}MachineArt)
    target_sources(${target} PRIVATE "${MACHINE_ART_DIR}/MachineArt.cpp")
    target_include_directories(${target} PRIVATE "${MACHINE_ART_DIR}")
endfunction()
