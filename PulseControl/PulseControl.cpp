
#include "PulseControl.h"

#include <ffglex/FFGLUtilities.h>

#include "../../versions/PulseControl/version.h"

ffglqs::PluginInstance p (
                                          PluginFactory< PulseControl >,// Create method, allows the host to create a new instance of the plugin
                                          "HV37",           // Plugin unique ID
                                          "Pulse Control",         // Plugin name
                                          2,                          // API major version number
                                          1,                          // API minor version number
                                          PLUGIN_VERSION_MAJOR,         // Plugin major version number
                                          PLUGIN_VERSION_MINOR,         // Plugin minor version number
                                          FF_EFFECT,                       // Plugin type
                                          "Control Pulse from Resolume",  // Plugin description
                                          "Joris de Jong // Hybrid Constructs"         // About
                                      );

PulseControl::PulseControl() :
	ffglqs::Effect( true ),
	socket( true ),
	dirty( false )
{
	AddParam( ffglqs::ParamEvent::Create( "Tap" ) );
	AddParam( ffglqs::ParamEvent::Create( "Resync" ) );
	AddHueColorParam( "Color" );
	
}

PulseControl::~PulseControl()
{
}

FFResult PulseControl::InitGL( const FFGLViewportStruct* viewPort )
{
	return CFFGLPlugin::InitGL( viewPort );
}

FFResult PulseControl::ProcessOpenGL( ProcessOpenGLStruct* inputTextures )
{
	if ( dirty )
	{
		juce::DynamicObject* jsonHeader = new juce::DynamicObject();
		jsonHeader->setProperty( "id", "pulsenet" );
		jsonHeader->setProperty( "type", "client" );
		jsonHeader->setProperty( "name", juce::SystemStats::getComputerName() );

		juce::DynamicObject* jsonObject = new juce::DynamicObject();

		//color
		float h = params[ 2 ]->GetValue();
		float s = params[ 3 ]->GetValue();
		float v = params[ 4 ]->GetValue();

		juce::Colour color = juce::Colour::fromHSV( h, s, v, 1.0f );
		if ( color != pColor )
		{ 
			jsonObject->setProperty( "color", color.toString() );
			pColor = color;
		}
		
		if ( params[ 0 ]->GetValue() > 0.5f )
		{
			jsonObject->setProperty( "tap", 1 );
			params[ 0 ]->SetValue( 0.0f );
		}

		if ( params[ 1 ]->GetValue() > 0.5f )
		{
			jsonObject->setProperty( "resync", 1 );
			params[ 1 ]->SetValue( 0.0f );
		}
		
		jsonHeader->setProperty( "data", juce::var( jsonObject ) );

		const juce::String message = juce::JSON::toString( juce::var ( jsonHeader ) );

		socket.write( "239.255.23.17", 51340, message.toRawUTF8(), message.getNumBytesAsUTF8() );

		dirty = false;
	}
	return FF_FAIL;
}

FFResult PulseControl::DeInitGL()
{
	return FF_SUCCESS;
}

FFResult PulseControl::SetFloatParameter( unsigned int index, float value )
{
	//first call the superclass implementation so the params get their values updated
	auto result = Effect::SetFloatParameter( index, value );

	dirty = true;
	return result;
}
