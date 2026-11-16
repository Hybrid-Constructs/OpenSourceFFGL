add_ffgl_plugin(PulseControl
VERSION
1.2.0
SOURCES
PulseControl.h
PulseControl.cpp
PulseNetAnnouncer.h
PulseNetAnnouncer.cpp
)

target_link_libraries(PulseControl PRIVATE 
	juce
)

set_target_properties(PulseControl PROPERTIES 
FOLDER "External"
)

