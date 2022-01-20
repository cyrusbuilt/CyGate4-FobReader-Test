#include <Arduino.h>
#include <Wire.h>

#define FIRMWARE_VERSION "1.0"

#define DEBUG_BAUD_RATE 9600

// Commands
#define FOBREADER_DETECT 0xFA
#define FOBREADER_INIT 0xFB
#define FOBREADER_GET_FIRMWARE 0xFC
#define FOBREADER_GET_TAGS 0xFD
#define FOBREADER_GET_AVAILABLE 0xFE
#define FOBREADER_SELF_TEST 0xDC
#define FOBREADER_DETECT_ACK 0xDA
#define FOBREADER_MIFARE_VERSION 0xDB
#define FOBREADER_BAD_CARD 0xDD

// Sizes
#define FOBREADER_FW_PREAMBLE_SIZE 2
#define FOBREADER_MAX_TAG_SIZE 10
#define FOBREADER_TAG_PRESENCE_SIZE 2
#define FOBREADER_TAG_DATA_SIZE 14
#define FOBREADER_MIFARE_VER_SIZE 2
#define FOBREADER_SELF_TEST_SIZE 2

void enterCLI();

// TODO Init status codes
// TODO Self-test status codes

typedef struct {
	uint8_t records;
	uint8_t tagBytes[FOBREADER_MAX_TAG_SIZE];
	uint8_t size;
	uint8_t id;
} Tag;

byte deviceAddr = 0xFF;
Tag tag;

void clearTag() {
	tag.id = 0xFF;
	tag.size = 0;
	tag.records = 0;
	for (uint8_t i = 0; i < FOBREADER_MAX_TAG_SIZE; i++) {
		tag.tagBytes[i] = 0xFF;
	}
}

void printHex(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
    	Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    	Serial.print(buffer[i], HEX);
  	}
}

void initSerial() {
	Serial.begin(DEBUG_BAUD_RATE);
	while (!Serial) {
		delay(1);
	}

	Serial.print(F("INIT: CyGate4-FobReader-Test v"));
	Serial.print(FIRMWARE_VERSION);
	Serial.println(F(" booting..."));
}

void initCommBus() {
	Serial.print(F("INIT: Initializing I2C comm bus... "));
	Wire.begin();
	Serial.println(F("DONE"));

	byte error;
    byte address;
    int devices = 0;

	Serial.println(F("INFO: Scanning I2C bus devices..."));
	for (address = 0; address < 127; address++) {
		Wire.beginTransmission(address);
		error = Wire.endTransmission();
		if (error == 0) {
			devices++;
			Serial.print(F("INFO: I2C device found at address 0x"));
			if (address < 16) {
				Serial.print(F("0"));
			}

			Serial.print(address, HEX);
			Serial.println(F("!"));

			deviceAddr = address;
		}
		else if (error == 4) {
			Serial.print(F("ERROR: Unknown error at address 0x"));
			if (address < 16) {
				Serial.print(F("0"));
			}

			Serial.println(address, HEX);
		}
	}

	if (devices == 0) {
		Serial.println(F("ERROR: No devices found!"));
	}
	else {
		Serial.println(F("INFO: Bus scan complete."));
		Serial.print(F("INFO: Found "));
		Serial.print(devices);
		Serial.println(F(" devices."));
	}
}

void writeByte(byte byte) {
	Wire.beginTransmission(deviceAddr);
	Wire.write(byte);
	Wire.endTransmission();
}

uint8_t readByte() {
	Wire.requestFrom(deviceAddr, (uint8_t)1);
	while (Wire.available() < 1) {
		delay(1);
	}

	return Wire.read();
}

byte* readBytes(size_t len) {
	byte* buffer = new byte[len];
	Wire.requestFrom(deviceAddr, (uint8_t)len);
	while (Wire.available() < 1) {
		delay(1);
	}

	for (size_t i = 0; i < len; i++) {
		buffer[i] = Wire.read();
	}

	return buffer;
}

bool detect() {
	Serial.print(F("sending detect to address: 0x"));
	if (deviceAddr < 16) {
		Serial.print(F("0"));
	}
	Serial.println(deviceAddr, HEX);

	writeByte(FOBREADER_DETECT);
	Serial.println(F("request data"));
	uint8_t response = readByte();
	Serial.print(F("DEBUG: Response = 0x"));
	Serial.println(response, HEX);

	return response == FOBREADER_DETECT_ACK;
}

bool doInit() {
	writeByte(FOBREADER_INIT);
	uint8_t response = readByte();
	Serial.print(F("DEBUG: Response = 0x"));
	Serial.println(response, HEX);
	return response == FOBREADER_INIT;
}

bool selfTest() {
	bool result = false;
	writeByte(FOBREADER_SELF_TEST);
	uint8_t* response = readBytes(FOBREADER_SELF_TEST_SIZE);
	Serial.print(F("DEBUG: response = "));
	printHex(response, FOBREADER_SELF_TEST_SIZE);
	Serial.println();
	if (response[0] == FOBREADER_SELF_TEST) {
		result = (bool)response[1];
	}

	delete[] response;
	return result;
}

String getFirmwareVersion() {
	String result = "";
	writeByte(FOBREADER_GET_FIRMWARE);

	byte* response = readBytes(FOBREADER_FW_PREAMBLE_SIZE);
	if (response[0] == FOBREADER_GET_FIRMWARE) {
		size_t len = response[1];

		// The second response is the actual version string in bytes.
		writeByte(FOBREADER_GET_FIRMWARE);
		unsigned int payloadSize = len + FOBREADER_FW_PREAMBLE_SIZE;
		byte *val = readBytes(payloadSize);
		for (unsigned int i = FOBREADER_FW_PREAMBLE_SIZE; i < payloadSize; i++) {
			if (val[i] != 0x0) {
				result += (char)val[i];
			}
		}

		delete[] val;
	}

	delete[] response;
	return result;
}

bool isNewTagPresent() {
	bool result = false;
	writeByte(FOBREADER_GET_AVAILABLE);
	uint8_t* response = readBytes(FOBREADER_TAG_PRESENCE_SIZE);
	Serial.print(F("DEBUG: Presence packet = "));
	printHex(response, FOBREADER_TAG_PRESENCE_SIZE);
	Serial.println();

	if (response[0] == FOBREADER_GET_AVAILABLE) {
		result = response[1] == 0x01;
	}

	delete[] response;
	return result;
}

bool getTagData() {
	bool result = false;
	writeByte(FOBREADER_GET_TAGS);
	uint8_t* response = readBytes(FOBREADER_TAG_DATA_SIZE);
	Serial.print(F("DEBUG: Tag data packet = "));
	printHex(response, FOBREADER_TAG_DATA_SIZE);
	Serial.println();
	if (response[0] == FOBREADER_GET_TAGS) {
		clearTag();
		tag.id = 0;
		tag.records = response[1];
		tag.size = response[2];
		for (uint8_t i = 0; i < tag.size; i++) {
			tag.tagBytes[i] = response[i + 3];
		}

		result = true;
	}

	delete[] response;
	return result;
}

byte getMiFareVersion() {
	byte result = 0xFF;
	writeByte(FOBREADER_MIFARE_VERSION);

	// Byte 0: 0xDB (command ack)
	// Byte 1: The MiFare firmware version code (ie. 0x92)
	byte *response = readBytes(FOBREADER_MIFARE_VER_SIZE);
	if (response[0] == FOBREADER_MIFARE_VERSION) {
		result = response[1];
	}

	delete[] response;
	return result;
}

String xlateMiFareVersion(byte version) {
	String result = "";
	switch (version) {
		case 0x88:
			result = "(clone)";
			break;
		case 0x90:
			result = "v0.0";
			break;
		case 0x91:
			result = "v1.0";
			break;
		case 0x92:
			result = "v2.0";
			break;
		case 0x12:
			result = "conterfeit chip";
			break;
		default:
			result = "unknown";
			break;
	}

	return result;
}

void initReaders() {
	Serial.print(F("INIT: Detecting prox readers... "));
	if (deviceAddr == 0xFF) {
		Serial.println(F("NONE FOUND"));
		Serial.println(F("DEBUG: No addresses"));
		return;
	}

	if (detect()) {
		Serial.print(F("INIT: Initializing fob reader at address 0x"));
		Serial.println(deviceAddr, HEX);
		if (!doInit()) {
			Serial.println(F("ERROR: Failed to initialize reader."));
		}
		else {
			Serial.println(F("INIT: CyGate4-FobReader detected and initialized."));
			Serial.print(F("INIT: Reader FW version = "));
			Serial.println(getFirmwareVersion());
			Serial.print(F("INIT: RFID Reader FW version = 0x"));
			byte ver = getMiFareVersion();
			Serial.print(ver, HEX);
			Serial.print(F(" - "));
			Serial.println(xlateMiFareVersion(ver));
		}
	}

	Serial.print(F("INIT: Finished initializing "));
	Serial.print(1);
	Serial.println(F(" readers."));
}

void waitForUserInput() {
	while (Serial.available() < 1) {
		delay(50);
	}
}

void printMenu() {
	Serial.println();
	Serial.println();
	Serial.println(F("**********************************"));
	Serial.println(F("*                                *"));
	Serial.println(F("* CyGate4-FobReader Test Program *"));
	Serial.println(F("*                                *"));
	Serial.println(F("*         Main Menu              *"));
	Serial.println(F("* a) Run RFID Self-Test          *"));
	Serial.println(F("* b) Request Tag Data            *"));
	Serial.println(F("* c) Restart                     *"));
	Serial.println(F("*                                *"));
	Serial.println(F("**********************************"));
	Serial.println();
	Serial.println("Enter selection (A/B/C): ");
	waitForUserInput();
}

void checkCommand() {
	String str = "";
	char incomingByte = Serial.read();
	switch (incomingByte) {
		case 'a':
			Serial.print(F("Result = "));
			Serial.println(selfTest() ? F("PASS") : F("FAIL"));
			break;
		case 'b':
			if (isNewTagPresent()) {
				Serial.println(F("Has tag data"));
				delay(2);
				if (getTagData()) {
					Serial.print(F("Tag = "));
					printHex(tag.tagBytes, tag.size);
					Serial.println();
				}
				else {
					Serial.println(F("Failed to retrieve tag data."));
				}
			}
			else {
				Serial.println(F("No tag data available."));
			}
			break;
		case 'c':
			setup();
			break;
		default:
			Serial.println(F("WARN: Unrecognized command."));
			break;
	}

	enterCLI();
}

void enterCLI() {
	printMenu();
	checkCommand();
}

void setup() {
	initSerial();
	initCommBus();
	initReaders();
	Serial.println(F("INIT: Boot sequence complete."));

	enterCLI();
}

void loop() {
	
}