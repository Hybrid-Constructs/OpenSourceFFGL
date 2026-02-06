add_ffgl_plugin(PulseControl
VERSION
1.1.0
SOURCES
PulseControl.h
PulseControl.cpp
)

target_link_libraries(PulseControl PRIVATE 
	juce
)

set_target_properties(PulseControl PROPERTIES 
FOLDER "External"
)

