
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <SmingCore/Digital.h>
#include <SmingCore/FileSystem.h>


// If you want, you can define WiFi settings globally in Eclipse Environment Variables

#define WIFI_SSID "Tenda_5EE5C0" // Put you SSID and Password here
#define WIFI_PWD "pandapanda"
#define IP_ADDRESS "192.168.1.172"

#define CONFIG_FILE "stations_config.txt"

// CONSTANTS

const char* STATIONS[] = {"Vltava", "Wave", "CRzurnal", "DDur", "CR2","JAZZ", "Junior", "Plus", "Regina", "Retro", "Sport"};
const uint8_t NUM_STATIONS = sizeof(STATIONS) / sizeof(char*);

#define MAX_STATIONS 20

typedef struct {
    String m_stations[MAX_STATIONS];
    uint8_t m_numStations;
    uint8_t m_currentStation;
} StationsConfig;

static StationsConfig g_StationsConfig = {0};
//    .m_stations = STATIONS,
//    .m_numStations = NUM_STATIONS,
//    .m_currentStation = 0,
//};

/// File desc with config of stations and current station as well
file_t g_File;

// PARAMENTERS

enum Event {
	PLAY = 0,
	PREVIOUS_STATION,
	NEXT_STATION,
	CONFIG, ///< Configuration pin (set stations)
};

const uint16_t PINS[] = {
        [PLAY] = 13,
        [PREVIOUS_STATION] = 12,
        [NEXT_STATION] = 4,
        [CONFIG] = 5,
};

// True in case of playing.
bool f_IsOn = false;


void SetPinTimeoutUs(uint16_t pin, uint32_t timeoutUs = 100000) {
	debugf("Pin %i toggle\n", pin);
	digitalWrite(pin, 1);
	delayMicroseconds(timeoutUs);
	digitalWrite(pin, 0);
}
void readConfigStations(void);
void configStations(void);
void playFunc(TcpClient& client);
void previousFunc(TcpClient& client);
void nextFunc(TcpClient& client);

void tcpServerClientConnected(TcpClient* client);
bool tcpServerClientReceive(TcpClient& client, char *data, int size);
void tcpServerClientComplete(TcpClient& client, bool succesfull);

TcpServer f_Server(tcpServerClientConnected, tcpServerClientReceive, tcpServerClientComplete);

void tcpServerClientConnected(TcpClient* client) {
	debugf("Application onClientCallback : %s\r\n",
			client->getRemoteIp().toString().c_str());
}

bool tcpServerClientReceive(TcpClient& client, char *data, int size) {
	debugf("Application DataCallback : %s, %d bytes, data: %s \r\n",
			client.getRemoteIp().toString().c_str(), size, data);
	// One byte information
	if (size == 2) {
		char dataInput = data[0];
		uint8_t value = atoi(&dataInput);
        switch (value) {
        case PLAY:
            Serial.println("Play request received.");
            playFunc(client);
            break;
        case PREVIOUS_STATION:
            Serial.println("Previous station request received.");
            previousFunc(client);
            break;
        case NEXT_STATION:
            Serial.println("Next station request received.");
            nextFunc(client);
            break;
        default:
            debugf("Unexpected data: %s\n", data);
            return false;
        }
	} else {
		debugf("Unexpected data length, %i\n", size);
		return false;
	}
	return true;
}

void playFunc(TcpClient& client) {
    SetPinTimeoutUs(PINS[PLAY]);
    f_IsOn = not (f_IsOn);
    if (f_IsOn) {
        client.sendString(g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]);
    }
}

void previousFunc(TcpClient& client) {
    if ((g_StationsConfig.m_currentStation > 0) && f_IsOn) {
        --g_StationsConfig.m_currentStation;
        SetPinTimeoutUs(PINS[PREVIOUS_STATION]);
        client.sendString(g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]);
        fileWrite(g_File, (const void*)&g_StationsConfig, sizeof(g_StationsConfig));
    } else {
        if (f_IsOn) {
            client.sendString(
                    "Cannot set previous station, already on the first one");
        } else {
            client.sendString("Turn on radio!");
        }
    }
}

void nextFunc(TcpClient& client) {
    if ((g_StationsConfig.m_currentStation < (g_StationsConfig.m_numStations - 1)) && f_IsOn) {
        ++g_StationsConfig.m_currentStation;
        SetPinTimeoutUs(PINS[NEXT_STATION]);
        client.sendString(g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]);
        fileWrite(g_File, (const void*) &g_StationsConfig, sizeof(g_StationsConfig));
    } else {
        if (f_IsOn) {
            client.sendString("Cannot set next station, already on the last one");
        } else {
            client.sendString("Turn on radio!");
        }
    }
}

void tcpServerClientComplete(TcpClient& client, bool succesfull) {
	debugf("Application CompleteCallback : %s \r\n", client.getRemoteIp().toString().c_str());
}

// Will be called when WiFi station was connected to AP
void connectOk() {
	debugf("I'm CONNECTED");
	Serial.println(WifiStation.getIP().toString());
	Serial.println("I'm CONNECTED");
	f_Server.listen(80);
	f_Server.setTimeOut(0xFFFF);
}




////////////////////////////////////////////////////////////////////////////////
// Will be called when WiFi station was connected to AP
void gotIP(IPAddress, IPAddress, IPAddress)
{
	Serial.println("I'm CONNECTED");
}

////////////////////////////////////////////////////////////////////////////////
// Will be called when WiFi station timeout was reached
void connectFail(String, uint8_t, uint8_t[6], uint8_t)
{
	Serial.println("I'm NOT CONNECTED. Need help :(");
}

// Will be called when WiFi hardware and software initialization was finished
// And system initialization was completed
void ready(void) {
	debugf("READY!");
	for (uint8_t i = 0; i < 3; ++i) {
		pinMode(PINS[i], OUTPUT);
		digitalWrite(PINS[i], 0);
	}
	pinMode(PINS[CONFIG], INPUT_PULLUP);
	readConfigStations();
	configStations();
}

void readConfigStations(void) {
    String configFile(CONFIG_FILE);
    // first time write config
    size_t status;
    if (!fileExist(configFile)) {
        g_File = fileOpen( configFile, eFO_WriteOnly);
        status = fileWrite(g_File,(const void*) &g_StationsConfig, sizeof(g_StationsConfig));
        if (status < 0) debugf("Write config failed, %d\n", status);
    } else {
        g_File = fileOpen(configFile, eFO_ReadWrite);
        status = fileRead(g_File, (void *) &g_StationsConfig, sizeof(g_StationsConfig));
        if (status < 0) debugf("Read config failed, %d\n", status);
    }
}

void configStations(void) {
    if (digitalRead(PINS[CONFIG]) != 0) return;
    String configFile(CONFIG_FILE);
    if (!fileExist(configFile)) return;
    Serial.println("Write stations from the first to the last");
    Serial.println("In case of last station written, type -q or ");
    String station;
    Serial.print( "Station 1: ");
    g_StationsConfig.m_numStations = 0;
    while((station = Serial.readStringUntil('\n')) != String("-q")) {
        g_StationsConfig.m_stations[g_StationsConfig.m_numStations++] = station;
        if (g_StationsConfig.m_numStations >= MAX_STATIONS) {
            Serial.printf("Max number of stations (%d) reached\n", MAX_STATIONS);
            break;
        }
        Serial.printf("Station %d: ", g_StationsConfig.m_numStations + 1);
    }
}

void init(void) {
	// Initialize wifi connection
    for (int i = 0; i < NUM_STATIONS; ++i) {
        g_StationsConfig.m_stations[i] = String(STATIONS[i]);
    }
    g_StationsConfig.m_numStations = NUM_STATIONS;
    g_StationsConfig.m_currentStation = 0;

	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial
	Serial.println("Sming. Let's do smart things!");

	// Set system ready callback method
	System.onReady(ready);

	// Station - WiFi client
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD); // Put you SSID and Password here

	// Optional: Change IP addresses (and disable DHCP)
	WifiStation.setIP(String(IP_ADDRESS));

	// Run our method when station was connected to AP (or not connected)
	// We recommend 20+ seconds at start

    WifiEvents.onStationGotIP(gotIP);
    WifiEvents.onStationDisconnect(connectFail);
}
