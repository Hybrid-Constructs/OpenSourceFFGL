#pragma once

#include <mutex>

#include <juce_core/juce_core.h>

// Shared PulseNet sender for all PulseControl instances in this process. Resolume commonly creates
// more than one PulseControl instance for what is conceptually a single connection from this machine
// (e.g. the same effect placed on multiple layers) - those should announce and send as one shared
// PulseNet client, not one heartbeat/socket per instance. Ref-counted the same way as
// HubLicenseChecker: addInstance()/removeInstance() called from each instance's InitGL/DeInitGL.
class PulseNetAnnouncer :
	private juce::Thread
{
public:
	PulseNetAnnouncer();
	~PulseNetAnnouncer();

	void addInstance();
	void removeInstance();

	// Queues a data send (tap/resync/color) for the next opportunity; also counts as the heartbeat.
	void send( juce::DynamicObject* data );

private:
	void run() override;
	void sendAnnounce();
	void sendData( juce::DynamicObject* data );

	std::mutex instanceMutex;
	int numInstances;

	std::unique_ptr<juce::DatagramSocket> socket;
	juce::CriticalSection sendLock;
	juce::Time lastAnnounceTime;
};
