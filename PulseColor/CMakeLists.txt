add_ffgl_plugin(PulseColor
VERSION
1.0.2
SOURCES
Pulse.h
Pulse.cpp
)

target_link_libraries(PulseColor PRIVATE 
	juce
)

set_target_properties(PulseColor PROPERTIES 
    FOLDER "External"
)

