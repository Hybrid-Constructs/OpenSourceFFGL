#include "PulseNetAnnouncer.h"

namespace
{
	constexpr const char* multicastIp = "239.255.23.17";
	constexpr int multicastPort = 51340;

	juce::DynamicObject* makeHeader()
	{
		auto* jsonHeader = new juce::DynamicObject();
		jsonHeader->setProperty( "id", "pulsenet" );
		jsonHeader->setProperty( "type", "client" );
		jsonHeader->setProperty( "name", juce::SystemStats::getComputerName() );
		jsonHeader->setProperty( "source", "ffgl" );
		return jsonHeader;
	}
}

PulseNetAnnouncer::PulseNetAnnouncer() :
	Thread( "PulseNet Announce" ),
	numInstances( 0 )
{
}

PulseNetAnnouncer::~PulseNetAnnouncer()
{
	// This is a DLL-scoped static, so this destructor only runs at DLL unload - see
	// HubLicenseChecker's dtor comment for why we don't attempt a stopThread() join from here.
	stopThread( 0 );
}

void PulseNetAnnouncer::addInstance()
{
	std::lock_guard<std::mutex> lock( instanceMutex );
	if ( numInstances == 0 )
	{
		socket.reset( new juce::DatagramSocket( true ) );
		lastAnnounceTime = juce::Time();
		startThread();
	}
	numInstances++;
}

void PulseNetAnnouncer::removeInstance()
{
	std::lock_guard<std::mutex> lock( instanceMutex );
	numInstances--;
	if ( numInstances == 0 )
	{
		stopThread( 1000 );
		socket.reset();
	}
}

void PulseNetAnnouncer::send( juce::DynamicObject* data )
{
	sendData( data );
}

void PulseNetAnnouncer::run()
{
	while ( !threadShouldExit() )
	{
		bool shouldAnnounce = false;
		{
			const juce::ScopedLock lock( sendLock );
			shouldAnnounce = ( juce::Time::getCurrentTime() - lastAnnounceTime ).inSeconds() > 3;
		}

		if ( shouldAnnounce )
			sendAnnounce();

		wait( 100 );
	}
}

void PulseNetAnnouncer::sendAnnounce()
{
	juce::DynamicObject::Ptr jsonHeader = makeHeader();
	const juce::String message = juce::JSON::toString( juce::var( jsonHeader.get() ) );

	const juce::ScopedLock lock( sendLock );
	if ( socket )
		socket->write( multicastIp, multicastPort, message.toRawUTF8(), message.getNumBytesAsUTF8() );
	lastAnnounceTime = juce::Time::getCurrentTime();
}

void PulseNetAnnouncer::sendData( juce::DynamicObject* data )
{
	juce::DynamicObject::Ptr jsonHeader = makeHeader();
	jsonHeader->setProperty( "data", juce::var( data ) );
	const juce::String message = juce::JSON::toString( juce::var( jsonHeader.get() ) );

	const juce::ScopedLock lock( sendLock );
	if ( socket )
		socket->write( multicastIp, multicastPort, message.toRawUTF8(), message.getNumBytesAsUTF8() );
	lastAnnounceTime = juce::Time::getCurrentTime();
}
