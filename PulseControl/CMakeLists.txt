add_ffgl_plugin(PulseControl
VERSION
1.3.0
SOURCES
PulseControl.h
PulseControl.cpp
DominantColor.h
PulseNetAnnouncer.h
PulseNetAnnouncer.cpp
)

target_link_libraries(PulseControl PRIVATE 
	juce
)

set_target_properties(PulseControl PROPERTIES 
FOLDER "External"
)

