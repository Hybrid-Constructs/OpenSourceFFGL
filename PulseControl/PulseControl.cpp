#include "PulseControl.h"

#include <ffglex/FFGLUtilities.h>

#include "PulseNetAnnouncer.h"
#include "../../versions/PulseControl/version.h"

namespace
{
	// DLL-scoped static, shared by every PulseControl instance in this process - see
	// PulseNetAnnouncer.h for why (Resolume commonly creates more than one instance for what is
	// conceptually a single connection from this machine).
	PulseNetAnnouncer announcer;

	// How often the input is reduced to a thumbnail and read back. 10 Hz is plenty for a UI colour.
	constexpr double captureIntervalMs = 100.0;
	// A queued readback is only mapped once the GPU has surely finished it, so mapping never stalls.
	constexpr double mapDelayMs = 30.0;
	// How far the smoothed colour has to move (0..1 per channel) before it is sent again.
	constexpr float sendThreshold = 3.0f / 255.0f;
	// Share of the way the smoothed colour moves toward each new sample.
	constexpr float smoothing = 0.5f;

	const char* thumbVertexShader = R"(#version 410 core
		uniform vec2 maxUV;

		layout( location = 0 ) in vec4 vPosition;
		layout( location = 1 ) in vec2 vUV;

		out vec2 i_uv;

		void main()
		{
			gl_Position = vPosition;
			i_uv = vUV * maxUV;
		}
	)";

	// One tap per thumbnail pixel. Averaging a block first would mix neighbouring colours and wash out
	// the saturation the dominant-colour step votes on; a handful of point samples tracks the
	// full-resolution dominant colour more closely (measured on real photos, 8x8 stays within ~1.4
	// degrees of hue).
	const char* thumbFragmentShader = R"(#version 410 core
		uniform sampler2D inputTexture;

		in vec2 i_uv;
		out vec4 fragColor;

		void main()
		{
			fragColor = texture( inputTexture, i_uv );
		}
	)";
}

static CFFGLPluginInfo PluginInfo(
	PluginFactory< PulseControl >,  // Create method, allows the host to create a new instance of the plugin
	"HV37",                         // Plugin unique ID
	"Pulse Control",                // Plugin name
	2,                               // API major version number
	1,                               // API minor version number
	PLUGIN_VERSION_MAJOR,           // Plugin major version number
	PLUGIN_VERSION_MINOR,           // Plugin minor version number
	FF_EFFECT,                      // Plugin type
	"Control Pulse from Resolume",  // Plugin description
	"Joris de Jong // Hybrid Constructs" // About
);

PulseControl::PulseControl() :
	CFFGLPlugin( true ),
	dirty( false ),
	tap( false ),
	resync( false ),
	hsva{ 0.0f, 0.0f, 1.0f, 1.0f },
	useInputColor( false ),
	viewport{},
	thumbPbo( 0 ),
	captureReady( false ),
	readbackPending( false ),
	lastCaptureMs( 0.0 ),
	haveInputColor( false )
{
	SetMinInputs( 1 );
	SetMaxInputs( 1 );

	SetParamInfo( TapParam, "Tap", FF_TYPE_EVENT, 0.0f );
	SetParamInfo( ResyncParam, "Resync", FF_TYPE_EVENT, 0.0f );
	SetParamInfo( HueParam, "Color", FF_TYPE_HUE, hsva[ 0 ] );
	SetParamInfo( SatParam, "Color_saturation", FF_TYPE_SATURATION, hsva[ 1 ] );
	SetParamInfo( ValParam, "Color_brightness", FF_TYPE_BRIGHTNESS, hsva[ 2 ] );
	SetParamInfo( AlphaParam, "Color_alpha", FF_TYPE_ALPHA, hsva[ 3 ] );
	SetParamInfo( InputColorParam, "Sample Color", FF_TYPE_BOOLEAN, 0.0f );
}

PulseControl::~PulseControl()
{
}

FFResult PulseControl::InitGL( const FFGLViewportStruct* viewPort )
{
	// called first, before any fallible step, so a failed InitGL's DeInitGL() path always balances
	announcer.addInstance();
	viewport = *viewPort;

	// Without the capture resources the plugin still works as the manual control it always was.
	captureReady = initCaptureGL();

	return CFFGLPlugin::InitGL( viewPort );
}

unsigned int PulseControl::Resize( const FFGLViewportStruct* viewPort )
{
	viewport = *viewPort;
	return FF_SUCCESS;
}

bool PulseControl::initCaptureGL()
{
	if ( !thumbShader.Compile( thumbVertexShader, thumbFragmentShader ) )
		return false;

	if ( !thumbQuad.Initialise() )
	{
		releaseCaptureGL();
		return false;
	}

	if ( !thumbFbo.Initialise( thumbSide, thumbSide, GL_RGBA8 ) )
	{
		releaseCaptureGL();
		return false;
	}

	glGenBuffers( 1, &thumbPbo );
	glBindBuffer( GL_PIXEL_PACK_BUFFER, thumbPbo );
	glBufferData( GL_PIXEL_PACK_BUFFER, thumbBytes, nullptr, GL_STREAM_READ );
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

	readbackPending = false;
	return true;
}

void PulseControl::releaseCaptureGL()
{
	thumbShader.FreeGLResources();
	thumbQuad.Release();
	thumbFbo.Release();
	if ( thumbPbo != 0 )
	{
		glDeleteBuffers( 1, &thumbPbo );
		thumbPbo = 0;
	}
	readbackPending = false;
}

bool PulseControl::collectThumbnail()
{
	if ( !readbackPending || juce::Time::getMillisecondCounterHiRes() - lastCaptureMs < mapDelayMs )
		return false;
	readbackPending = false;

	std::array<uint8_t, thumbBytes> thumb;
	glBindBuffer( GL_PIXEL_PACK_BUFFER, thumbPbo );
	const void* mapped = glMapBufferRange( GL_PIXEL_PACK_BUFFER, 0, thumbBytes, GL_MAP_READ_BIT );
	if ( mapped == nullptr )
	{
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		return false;
	}
	memcpy( thumb.data(), mapped, thumbBytes );
	glUnmapBuffer( GL_PIXEL_PACK_BUFFER );
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

	const auto result = pulsecolor::dominantColour( thumb.data(), thumbSide * thumbSide );

	if ( !haveInputColor )
	{
		inputColor = sentInputColor = result.colour;
		haveInputColor = true;
		return true;
	}

	inputColor = pulsecolor::smoothToward( inputColor, result.colour, smoothing );
	if ( !pulsecolor::differsEnough( sentInputColor, inputColor, sendThreshold ) )
		return false;

	sentInputColor = inputColor;
	return true;
}

void PulseControl::captureThumbnail( ProcessOpenGLStruct* inputTextures )
{
	if ( inputTextures->numInputTextures < 1 || inputTextures->inputTextures[ 0 ] == nullptr )
		return;

	const double now = juce::Time::getMillisecondCounterHiRes();
	if ( readbackPending || now - lastCaptureMs < captureIntervalMs )
		return;
	lastCaptureMs = now;

	const FFGLTextureStruct& source = *inputTextures->inputTextures[ 0 ];
	const FFGLTexCoords maxCoords = GetMaxGLTexCoords( source );

	glBindFramebuffer( GL_FRAMEBUFFER, thumbFbo.GetGLID() );
	glViewport( 0, 0, thumbSide, thumbSide );
	{
		ffglex::ScopedShaderBinding shaderBinding( thumbShader.GetGLID() );
		ffglex::ScopedSamplerActivation activateSampler0( 0 );
		ffglex::Scoped2DTextureBinding textureBinding0( source.Handle );

		thumbShader.Set( "inputTexture", 0 );
		thumbShader.Set( "maxUV", maxCoords.s, maxCoords.t );
		thumbQuad.Draw();
	}

	// With a pack buffer bound, glReadPixels only queues the copy; the data is mapped on a later frame.
	glBindBuffer( GL_PIXEL_PACK_BUFFER, thumbPbo );
	glReadPixels( 0, 0, thumbSide, thumbSide, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	readbackPending = true;

	glBindFramebuffer( GL_FRAMEBUFFER, inputTextures->HostFBO );
	glViewport( 0, 0, viewport.width, viewport.height );
}

FFResult PulseControl::ProcessOpenGL( ProcessOpenGLStruct* inputTextures )
{
	if ( useInputColor && captureReady )
	{
		if ( collectThumbnail() )
		{
			// The detected colour goes into the Color params, so Resolume's own controls follow it and
			// the colour sent below is exactly the one shown there.
			juce::Colour::fromFloatRGBA( inputColor.r, inputColor.g, inputColor.b, 1.0f ).getHSB( hsva[ 0 ], hsva[ 1 ], hsva[ 2 ] );
			RaiseParamEvent( HueParam, FF_EVENT_FLAG_VALUE );
			RaiseParamEvent( SatParam, FF_EVENT_FLAG_VALUE );
			RaiseParamEvent( ValParam, FF_EVENT_FLAG_VALUE );
			dirty = true;
		}
		captureThumbnail( inputTextures );
	}

	if ( dirty )
	{
		juce::DynamicObject* jsonObject = new juce::DynamicObject();

		// Until the first thumbnail has come back there is no input colour to send.
		if ( !useInputColor || haveInputColor )
		{
			juce::Colour color = juce::Colour::fromHSV( hsva[ 0 ], hsva[ 1 ], hsva[ 2 ], 1.0f );
			if ( color != pColor )
			{
				jsonObject->setProperty( "color", color.toString() );
				pColor = color;
			}
		}

		if ( tap )
		{
			jsonObject->setProperty( "tap", 1 );
			tap = false;
		}

		if ( resync )
		{
			jsonObject->setProperty( "resync", 1 );
			resync = false;
		}

		announcer.send( jsonObject );

		dirty = false;
	}
	return FF_FAIL;
}

FFResult PulseControl::DeInitGL()
{
	releaseCaptureGL();
	captureReady = false;
	announcer.removeInstance();
	return FF_SUCCESS;
}

FFResult PulseControl::SetFloatParameter( unsigned int index, float value )
{
	switch ( index )
	{
	case ( TapParam ):
		tap = value > 0.5f;
		break;
	case ( ResyncParam ):
		resync = value > 0.5f;
		break;
	case ( HueParam ):
	case ( SatParam ):
	case ( ValParam ):
	case ( AlphaParam ):
		hsva[ index - HueParam ] = value;
		break;
	case ( InputColorParam ):
		{
			const bool on = value > 0.5f;
			if ( on != useInputColor )
			{
				useInputColor = on;
				// start the smoothing from scratch, so it doesn't glide from a stale colour
				haveInputColor = false;
				readbackPending = false;
			}
		}
		break;
	default:
		return FF_FAIL;
	}

	dirty = true;
	return FF_SUCCESS;
}

float PulseControl::GetFloatParameter( unsigned int index )
{
	switch ( index )
	{
	case ( TapParam ):
		return tap ? 1.0f : 0.0f;
	case ( ResyncParam ):
		return resync ? 1.0f : 0.0f;
	case ( HueParam ):
	case ( SatParam ):
	case ( ValParam ):
	case ( AlphaParam ):
		return hsva[ index - HueParam ];
	case ( InputColorParam ):
		return useInputColor ? 1.0f : 0.0f;
	default:
		return 0.0f;
	}
}
