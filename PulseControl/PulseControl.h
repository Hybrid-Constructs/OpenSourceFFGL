#pragma once

#include <FFGLSDK.h>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

class PulseControl :
	public CFFGLPlugin
{
public:
	PulseControl();
	~PulseControl();

	FFResult InitGL( const FFGLViewportStruct* viewPort ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* inputTextures ) override;
	FFResult DeInitGL() override;

	//we use these as callbacks to know when a param has been changed in Resolume
	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	enum ParamIndex
	{
		TapParam = 0,
		ResyncParam,
		HueParam,
		SatParam,
		ValParam,
		AlphaParam
	};

	juce::CriticalSection paramLock;
	bool dirty;

	bool tap;
	bool resync;
	float hsva[ 4 ];

	juce::Colour pColor;
};
