#pragma once

#include <FFGLSDK.h>

#include <array>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "DominantColor.h"

class PulseControl :
	public CFFGLPlugin
{
public:
	PulseControl();
	~PulseControl();

	FFResult InitGL( const FFGLViewportStruct* viewPort ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* inputTextures ) override;
	FFResult DeInitGL() override;
	unsigned int Resize( const FFGLViewportStruct* viewPort ) override;

	//we use these as callbacks to know when a param has been changed in Resolume
	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	// New params are only ever appended, so saved Resolume compositions keep their indices.
	enum ParamIndex
	{
		TapParam = 0,
		ResyncParam,
		HueParam,
		SatParam,
		ValParam,
		AlphaParam,
		InputColorParam
	};

	// The input is reduced to a thumbnail of this many pixels square on the GPU, then read back.
	// 8 is the floor: at 4 the hue is off by up to ~17 degrees on real photos, at 2 it can flip entirely.
	static constexpr int thumbSide = 8;
	static constexpr int thumbBytes = thumbSide * thumbSide * 4;

	bool initCaptureGL();
	void releaseCaptureGL();
	// Reads back the thumbnail captured by an earlier frame, if there is one. True when the analysed colour moved.
	bool collectThumbnail();
	// Renders the input down to the thumbnail and starts its asynchronous readback, if one is due.
	void captureThumbnail( ProcessOpenGLStruct* inputTextures );

	juce::CriticalSection paramLock;
	bool dirty;

	bool tap;
	bool resync;
	float hsva[ 4 ];
	bool useInputColor;

	juce::Colour pColor;

	FFGLViewportStruct viewport;
	ffglex::FFGLShader thumbShader;
	ffglex::FFGLScreenQuad thumbQuad;
	ffglex::FFGLFBO thumbFbo;
	GLuint thumbPbo;
	bool captureReady;
	bool readbackPending;
	double lastCaptureMs;

	pulsecolor::Rgb inputColor;
	bool haveInputColor;
	pulsecolor::Rgb sentInputColor;
};
