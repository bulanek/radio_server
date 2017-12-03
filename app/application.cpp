#include <user_config.h>
#include <SmingCore/SmingCore.h>


// If you want, you can define WiFi settings globally in Eclipse Environment Variables

#define WIFI_SSID "Tenda_5EE5C0" // Put you SSID and Password here
#define WIFI_PWD "pandapanda"
#define IP_ADDRESS "192.168.1.172"


// CONSTANTS

const char* STATIONS[] = {"Vltava", "Wave", "CRzurnal", "DDur", "CR2","JAZZ",
		"Junior", "Plus", "Regina", "Retro", "Sport"};
const uint8_t NUM_STATIONS = 11;


const uint8_t PINS[] = { 13, 12, 4};

// PARAMENTERS

enum Event
{
	PLAY = 0,
	PREVIOUS_STATION,
	NEXT_STATION,
	NUM_EVENTS
} f_Event = NUM_EVENTS;

uint8_t f_CurrentStation = 0;
// True in case of playing.
bool f_IsOn = false;


////////////////////////////////////////////////////////////////////////////////
// FUNCTION NAME: SetPinTimeoutUs
////////////////////////////////////////////////////////////////////////////////
void SetPinTimeoutUs(uint16_t pin, uint32_t timeoutUs = 100000) {
	debugf("Pin %i toggle\n", pin);
	digitalWrite(pin, 1);
	delayMicroseconds(timeoutUs);
	digitalWrite(pin, 0);
}

void tcpServerClientConnected(TcpClient* client);
bool tcpServerClientReceive(TcpClient& client, char *data, int size);
void tcpServerClientComplete(TcpClient& client, bool succesfull);

TcpServer f_Server(tcpServerClientConnected, tcpServerClientReceive,
		tcpServerClientComplete);

////////////////////////////////////////////////////////////////////////////////
// FUNCTION NAME: tcpServerClientConnected
////////////////////////////////////////////////////////////////////////////////
void tcpServerClientConnected(TcpClient* client) {
	debugf("Application onClientCallback : %s\r\n",
			client->getRemoteIp().toString().c_str());
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION NAME: tcpServerClientReceive
////////////////////////////////////////////////////////////////////////////////
bool tcpServerClientReceive(TcpClient& client, char *data, int size) {
	debugf("Application DataCallback : %s, %d bytes \r\n",
			client.getRemoteIp().toString().c_str(), size);
	// One byte information
	if (size == 2) {
		char dataInput = data[0];
		uint8_t value = atoi(&dataInput);
		debugf("Value: %i\n", value);
        switch (value)
        {
        case PLAY:
            Serial.println("Play request received.");
            SetPinTimeoutUs(PINS[PLAY]);
            f_IsOn = not(f_IsOn);
            if (f_IsOn)
            {
            	client.sendString(STATIONS[f_CurrentStation]);
            }
            break;
        case PREVIOUS_STATION:
            if ((f_CurrentStation > 0) && f_IsOn)
            {
                --f_CurrentStation;
                Serial.println("Previous station request received.");
                SetPinTimeoutUs(PINS[PREVIOUS_STATION]);
                client.sendString(STATIONS[f_CurrentStation]);
            }
            else
            {
				if (f_IsOn) {
					client.sendString(
							"Cannot set previous station, already on the first one");
				} else {
					client.sendString("Turn on radio");
				}
            }
            break;
        case NEXT_STATION:
            if ((f_CurrentStation < (NUM_STATIONS - 1)) && f_IsOn)
            {
                ++f_CurrentStation;
                Serial.println("Next station request received.");
                SetPinTimeoutUs(PINS[NEXT_STATION]);
                client.sendString(STATIONS[f_CurrentStation]);
            }
            else
            {
				if (f_IsOn) {
					client.sendString(
							"Cannot set next station, already on the last one");
				} else {
					client.sendString("Turn on radio");
				}
            }
            break;

        default:
            debugf("Data char: %s\n", data);
            return false;
        }
	}else{
		debugf("Unexpected data length, %i\n", size);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION NAME: tcpServerClientComplete
////////////////////////////////////////////////////////////////////////////////
void tcpServerClientComplete(TcpClient& client, bool succesfull) {
	debugf("Application CompleteCallback : %s \r\n",
			client.getRemoteIp().toString().c_str());
}


// Will be called when WiFi station was connected to AP
void connectOk() {
	debugf("I'm CONNECTED");
	Serial.println(WifiStation.getIP().toString());
	Serial.println("I'm CONNECTED");
	f_Server.listen(80);
	f_Server.setTimeOut(0xFFFF);
}


// Will be called when WiFi station timeout was reached
void connectFail() {
	debugf("I'm NOT CONNECTED!");
	WifiStation.waitConnection(connectOk, 10, connectFail); // Repeat and check again
}

// Will be called when WiFi hardware and software initialization was finished
// And system initialization was completed
void ready() {
	debugf("READY!");
	for (uint8_t i = 0; i < NUM_EVENTS; ++i) {
		pinMode(PINS[i], OUTPUT);
		digitalWrite(PINS[i], 0);
	}
}

void init() {
	// Initialize wifi connection
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial
	Serial.println("Sming. Let's do smart things!");

	// Set system ready callback method
	System.onReady(ready);

	// Station - WiFi client
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD); // Put you SSID and Password here

	// Optional: Change IP addresses (and disable DHCP)
	WifiStation.setIP(IPAddress(192, 168, 1, 172));

	// Run our method when station was connected to AP (or not connected)
	// We recommend 20+ seconds at start
	WifiStation.waitConnection(connectOk, 30, connectFail);
}
