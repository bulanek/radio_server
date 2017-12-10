
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
const uint8_t STATION_BUFFER_SIZE = 20;

#define MAX_STATIONS 20

#define DATA_ADDRESS 0x3f000
#define DATA_SECTOR 0x3f

typedef struct {
    uint8_t m_numStations;
    uint8_t m_currentStation;
    char* m_stations[MAX_STATIONS][STATION_BUFFER_SIZE];
} StationsConfig;

static StationsConfig g_StationsConfig = {0};


/// File desc with config of stations and current station as well
file_t g_File;

// PARAMETERS

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

String g_UartOutputStation;
uint8_t g_UartCurrentStation = 0;

ICACHE_FLASH_ATTR void flash_erase_all() {
    char settings[sizeof(StationsConfig)];
    for (int n = 0; n < sizeof(StationsConfig); n++) settings[n] = 0;
    spi_flash_erase_sector(DATA_SECTOR);
}

ICACHE_FLASH_ATTR void flash_write( const StationsConfig& stationsConfig) {
    SpiFlashOpResult result = spi_flash_erase_sector(DATA_SECTOR);
    if (result != SPI_FLASH_RESULT_OK) Serial.printf("Erase failed: %i\n\r", result);
    spi_flash_write(DATA_ADDRESS, (uint32_t*)&stationsConfig, sizeof(StationsConfig));
}

void SetPinTimeoutUs(uint16_t pin, uint32_t timeoutUs = 100000) {
	debugf("Pin %i toggle\n", pin);
	digitalWrite(pin, 1);
	delayMicroseconds(timeoutUs);
	digitalWrite(pin, 0);
}

void readSavedConfigStations(void);
void configStationsUART(void);
void playFunc(TcpClient& client);
void previousFunc(TcpClient& client);
void nextFunc(TcpClient& client);

/// Callback called while received byte by uart
void uartDelegate(Stream &source, char arrivedChar, uint16_t availableCharsCount);

void tcpServerClientConnected(TcpClient* client);
bool tcpServerClientReceive(TcpClient& client, char *data, int size);
void tcpServerClientComplete(TcpClient& client, bool succesfull);

TcpServer f_Server(tcpServerClientConnected, tcpServerClientReceive, tcpServerClientComplete);

void tcpServerClientConnected(TcpClient* client) {
	debugf("Application onClientCallback : %s\r\n", client->getRemoteIp().toString().c_str());
}

bool tcpServerClientReceive(TcpClient& client, char *data, int size) {
	debugf("Application DataCallback : %s, %d bytes, data: %s \r\n", client.getRemoteIp().toString().c_str(), size, data);
	if (size != 2) {
        debugf("Unexpected data length, %i\n", size);
		return false;
	}

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
	return true;
}

void playFunc(TcpClient& client) {
    SetPinTimeoutUs(PINS[PLAY]);
    f_IsOn = not (f_IsOn);
    if (f_IsOn) client.sendString(String((const char*)g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]));
}

void previousFunc(TcpClient& client) {
    if (!f_IsOn) {client.sendString("Turn on radio!"); return;}

    if (g_StationsConfig.m_currentStation > 0) {
        --g_StationsConfig.m_currentStation;
        SetPinTimeoutUs(PINS[PREVIOUS_STATION]);
        client.sendString(String((const char*)g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]));
        fileWrite(g_File, (const void*)&g_StationsConfig, sizeof(g_StationsConfig));
    } else {
        client.sendString("Cannot set previous station, already on the first one");
    }
}

void nextFunc(TcpClient& client) {
    if (!f_IsOn) {client.sendString("Turn on radio!"); return;}

    if (g_StationsConfig.m_currentStation < (g_StationsConfig.m_numStations - 1)) {
        ++g_StationsConfig.m_currentStation;
        SetPinTimeoutUs(PINS[NEXT_STATION]);
        client.sendString(String((const char*)g_StationsConfig.m_stations[g_StationsConfig.m_currentStation]));
        fileWrite(g_File, (const void*) &g_StationsConfig, sizeof(g_StationsConfig));
    } else {
        client.sendString("Cannot set next station, already on the last one");
    }
}

void uartDelegate(Stream &source, char arrivedChar,
        uint16_t availableCharsCount) {
    // Type char
    if (arrivedChar != '\r') {
        g_UartOutputStation += arrivedChar;
        Serial.print(arrivedChar);
    // exit
    } else if (g_UartOutputStation == String("-q")) {
        flash_erase_all();
        flash_write(g_StationsConfig);
        Serial.println("Write flash");
        Serial.printf("Num stations: %i\r\n", g_StationsConfig.m_numStations);
        Serial.printf("current station: %i\r\n", g_StationsConfig.m_currentStation);
        g_UartOutputStation.setString("");
        g_UartCurrentStation = 0;
        return;
    // Type enter
    } else {
        Serial.printf("\n\rStation selected: %s\n\r", g_UartOutputStation.c_str());
        memcpy((void*) g_StationsConfig.m_stations[g_UartCurrentStation], (void*) g_UartOutputStation.c_str(),
                (size_t) g_UartOutputStation.length() + 1);
        g_StationsConfig.m_numStations = ++g_UartCurrentStation;
        if (g_UartCurrentStation >= MAX_STATIONS) return;
        Serial.printf("\n\rStation %i: ", g_UartCurrentStation + 1);
        g_UartOutputStation.setString("");
    }
}

void tcpServerClientComplete(TcpClient& client, bool succesfull) {
	debugf("Application CompleteCallback : %s \r\n", client.getRemoteIp().toString().c_str());
}

void gotIP(IPAddress ip, IPAddress mask, IPAddress gateway) {
	debugf("Connected: ip (%s), mask(%s), gateway(%s)\n", ip.toString().c_str(), mask.toString().c_str(), gateway.toString().c_str());
	f_Server.listen(80);
	f_Server.setTimeOut(0xFFFF);
	readSavedConfigStations();
	configStationsUART();
}

void disconnect(String ssid, uint8_t ssid_len, uint8_t bssid[6], uint8_t reason) {
	Serial.printf("Disconnected: SSID : %s, reason: %i\n", ssid.c_str(), reason);
}

/// Will be called when WiFi hardware and software initialization was finished
/// And system initialization was completed
void ready(void) {
	debugf("READY!");
	for (uint8_t i = 0; i < 3; ++i) {
		pinMode(PINS[i], OUTPUT);
		digitalWrite(PINS[i], 0);
	}
	pinMode(PINS[CONFIG], INPUT_PULLUP);
    debugf("State of config pin: %i", digitalRead(PINS[CONFIG]));
}

void readSavedConfigStations(void) {
    StationsConfig configTmp;
    SpiFlashOpResult readResult = spi_flash_read(DATA_ADDRESS, (uint32_t*) &g_StationsConfig, sizeof(StationsConfig));
    if (readResult != SPI_FLASH_RESULT_OK) Serial.printf("Read flash failed, %i\r\n", readResult);
    if (g_StationsConfig.m_numStations >= MAX_STATIONS) g_StationsConfig.m_numStations = 0;

    Serial.println("\rSaved stations: ");
    for (int i = 0; i < g_StationsConfig.m_numStations; ++i) {
        Serial.printf("\tStation %i: %s\n\r", i + 1, g_StationsConfig.m_stations[i]);
    }
}

void configStationsUART(void) {
    if (digitalRead(PINS[CONFIG]) != 0) return;
    // To pass all callbacks
    g_StationsConfig.m_numStations = 0;
    g_StationsConfig.m_currentStation = 0;
    Serial.printf("\r\nStation 1: ");
}

void init(void) {
    for (int i = 0; i < NUM_STATIONS; ++i) memcpy((void*) g_StationsConfig.m_stations[i], STATIONS[i], strlen(STATIONS[i] + 1));
    g_StationsConfig.m_numStations = NUM_STATIONS;
    g_StationsConfig.m_currentStation = 0;

	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial
    Serial.setCallback(uartDelegate);

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
    WifiEvents.onStationDisconnect(disconnect);
}
