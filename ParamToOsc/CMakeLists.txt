add_ffgl_plugin(ParamToOsc
	VERSION
	1.0.1
	SOURCES
	ParamToOsc.h
	ParamToOsc.cpp

	${CMAKE_SOURCE_DIR}/modules/juce/modules/juce_osc/juce_osc.cpp
)

target_link_libraries(ParamToOsc PRIVATE 
	juce
)
set_target_properties(ParamToOsc PROPERTIES 
	FOLDER "External"
)

