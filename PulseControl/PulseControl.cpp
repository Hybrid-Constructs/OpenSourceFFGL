
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
	hsva{ 0.0f, 0.0f, 1.0f, 1.0f }
{
	SetParamInfo( TapParam, "Tap", FF_TYPE_EVENT, 0.0f );
	SetParamInfo( ResyncParam, "Resync", FF_TYPE_EVENT, 0.0f );
	SetParamInfo( HueParam, "Color", FF_TYPE_HUE, hsva[ 0 ] );
	SetParamInfo( SatParam, "Color_saturation", FF_TYPE_SATURATION, hsva[ 1 ] );
	SetParamInfo( ValParam, "Color_brightness", FF_TYPE_BRIGHTNESS, hsva[ 2 ] );
	SetParamInfo( AlphaParam, "Color_alpha", FF_TYPE_ALPHA, hsva[ 3 ] );
}

PulseControl::~PulseControl()
{
}

FFResult PulseControl::InitGL( const FFGLViewportStruct* viewPort )
{
	// called first, before any fallible step, so a failed InitGL's DeInitGL() path always balances
	announcer.addInstance();
	return CFFGLPlugin::InitGL( viewPort );
}

FFResult PulseControl::ProcessOpenGL( ProcessOpenGLStruct* inputTextures )
{
	if ( dirty )
	{
		juce::DynamicObject* jsonObject = new juce::DynamicObject();

		juce::Colour color = juce::Colour::fromHSV( hsva[ 0 ], hsva[ 1 ], hsva[ 2 ], 1.0f );
		if ( color != pColor )
		{
			jsonObject->setProperty( "color", color.toString() );
			pColor = color;
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
	default:
		return 0.0f;
	}
}
