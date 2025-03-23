#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <QuickEspNow.h>
#include <RadioLib.h>

#define MAXLEN 512

SX1262 radio = new Module(LoRa_nss, LoRa_dio1, LoRa_nrst, LoRa_busy); // SX1262

void printHexBuffer(const uint8_t* buffer, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (buffer[i] < 0x10) Serial.print('0');  // leading zero for single-digit hex
		Serial.print(buffer[i], HEX);
		Serial.print(' ');
		if ((i + 1) % 16 == 0) Serial.println(); // new line every 16 bytes (optional)
	}
	Serial.println(); // final newline
}


volatile bool loraReceived = false;
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetReceived() {
	loraReceived = true;
}

void dataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
	Serial.println("[ESPNow] Received message:");
	printHexBuffer(data, len);
	if (len>1) {
		if ((data[0] & 0b11000000) == 0b01000000)  {     // check protocol version 
			if ((data[1] & 0b00000011)) {
				radio.transmit(data, len);
				Serial.print("[LoRa] Sending packet:");
				printHexBuffer(data, len);
				loraReceived = false; // ignore receive interrupt as this is also triggered on transmit
				radio.startReceive();
			} else {
				Serial.println("[ESPNow] ERROR: 1st element is 0b00");
			}
		} else {
			Serial.println("[ESPNow] ERROR: wrong morsecode protocol!");
		}
	} else {
		Serial.println("[ESPNow] ERROR: espnow packet too small!");
	}
	Serial.println("=================");
}

void onLoraReceive(){
	int packetsize = radio.getPacketLength();
	if (packetsize == 0) return;
	Serial.print("[LoRa] Packet received, size: ");
	Serial.println(packetsize);
	uint8_t result[MAXLEN];
	int state = radio.readData(result, packetsize > MAXLEN ? MAXLEN : packetsize);
	if (state == RADIOLIB_ERR_NONE) {
		Serial.println("[LoRa] Packet received, RSSI: ");
		Serial.println(radio.getRSSI());
		Serial.print("[LoRa] Packet data: "); printHexBuffer(result, packetsize);
		if ((result[0] & 0b11000000) == 0b01000000)  {     // check protocol version 
			if ((result[1] & 0b00000011)) {
				Serial.println("[ESPNow] Sending message");
				quickEspNow.send (ESPNOW_BROADCAST_ADDRESS, result, packetsize);
				printHexBuffer(result, packetsize);
			} else {
				Serial.println("[LoRa] ERROR: 1st element is 0b00");
			}
		} else {
			Serial.println("[LoRa] ERROR: wrong protocol!");
		}
	} else {
		Serial.println("[LoRa] ERROR: Radiolib error!");
	}
	Serial.println("=================");
}



void setup () {
	Serial.begin (115200);
	delay(5000);
	Serial.println("LORA ESPNOW Gateway startup");
	Serial.println("[EspNow] Set station mode");
	WiFi.mode (WIFI_MODE_STA);
	WiFi.disconnect (false);
	quickEspNow.onDataRcvd (dataReceived);
	Serial.println("[EspNow] Start espnow");
	quickEspNow.begin (ESPNOW_CH); // If you are not connected to WiFi, channel should be specified
	Serial.println("[LoRa] Start SPI");
	SPI.begin(LoRa_SCK, LoRa_MISO, LoRa_MOSI, LoRa_nss);
	Serial.println(F("[LoRa] SX12xx initializing ... "));
	int state = radio.begin();  
	if (state != RADIOLIB_ERR_NONE) Serial.println("[LoRa] ERROR: radiolib begin()");
	radio.setFrequency(LORA_QRG/1000000.0);
	radio.setBandwidth(250.0);
	radio.setSpreadingFactor(7);
	radio.setSyncWord(LORA_SYNCWORD); // alternative 0x66
	radio.setOutputPower(LORA_POWER); // 14 = 25mW
	radio.setCRC(false);
	radio.setPacketReceivedAction(packetReceived);
	radio.startReceive();
	Serial.println("Setup complete");
}

void loop () {
	if(loraReceived) {
		onLoraReceive();
		loraReceived=false;
	}
}
