// On Linux to access the serial port, the following command is necessary:
// sudo chmod a+rw /dev/ttyACM0

#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h" // https://github.com/coryjfowler/MCP_CAN_lib
#include "MCP4151.h"

// Define Pins
#define SCK 13
#define MOSI 11
#define MISO 12
#define CS_CAN 8
#define CS_POTI 7
#define INTERRUPT_CAN 20
#define TEST_PIN 2

// Define CAN ID
#define msgIDSteeringWheelKeys 0x5C1

// Define CAN values for keys
#define CAN_NOKEY 0
#define CAN_VOL_UP 6
#define CAN_VOL_DOWN 7
#define CAN_HANG_UP 26
#define CAN_JOKER 43

// Define mapping Key -> Resistance
#define RES_NOKEY 100*1000
#define RES_VOL_UP 1*1000
#define RES_VOL_DOWN 2*1000
#define RES_HANG_UP 3*1000
#define RES_HANG_UP_LONG 5*1000
#define RES_JOKER 4*1000
#define RES_JOKER_LONG 6*1000


// Define MCP2515 and MCP4151
MCP4151 POTI(CS_POTI, MOSI, MISO, SCK);
MCP_CAN CAN(CS_CAN);

// Define CAN message variables
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];



void keyAction(uint8_t pressedKey) {
    static uint8_t lastPressedKey = 0;
    static unsigned long buttonPressStartTime = 0;
    static unsigned long lastLongPressTime = 0; // cooldown timer variable
    const unsigned long longPressDuration = 700;
    const unsigned long cooldownDuration = 1000; // 1 second cooldown after a long press

    // Check if we are within the cooldown period
    if (millis() - lastLongPressTime < cooldownDuration) {
        return; // Ignore all button presses during cooldown
    }

    switch(pressedKey) {
        case CAN_NOKEY:
            if (millis() - buttonPressStartTime < longPressDuration) {
                if (lastPressedKey == CAN_JOKER) POTI.setResistance(RES_JOKER);
                if (lastPressedKey == CAN_HANG_UP) POTI.setResistance(RES_HANG_UP);
            }
            buttonPressStartTime = 0;
            break;
        case CAN_VOL_UP:
             POTI.setResistance(RES_VOL_UP); // immediately send short press VOL UP
             buttonPressStartTime = 0; // reset long press timer
             break;
        case CAN_VOL_DOWN:
             POTI.setResistance(RES_VOL_DOWN); // immediately send short press VOL DOWN
             buttonPressStartTime = 0; // reset long press timer
             break;
        case CAN_JOKER:
        case CAN_HANG_UP:
            if (lastPressedKey == CAN_NOKEY) {
                buttonPressStartTime = millis(); // Start long press timer
            } else if (millis() - buttonPressStartTime >= longPressDuration) {
                if (pressedKey == CAN_JOKER) {
                    POTI.setResistance(RES_JOKER_LONG);
                    lastLongPressTime = millis(); // Update cooldown timestamp
                }
                if (pressedKey == CAN_HANG_UP) {
                    POTI.setResistance(RES_HANG_UP_LONG);
                    lastLongPressTime = millis(); // Update cooldown timestamp
                }
                buttonPressStartTime = 0; // Reset after long press
            }
            break;
        default:
            Serial.print("Unknown key pressed:");
            Serial.println(pressedKey);
            break;
    }

    // Additional debug output to trace values
    Serial.print("Last Pressed Key: ");
    Serial.println(lastPressedKey);
    Serial.print("Current Key: ");
    Serial.println(pressedKey);
    Serial.print("Time Since Last Press: ");
    Serial.println(millis() - buttonPressStartTime);

    delay(100);
    POTI.setResistance(RES_NOKEY);
    lastPressedKey = pressedKey;
}


void setup() {
    // Initialize Serial communication
    Serial.begin(9600);

    // Initialize SPI
    SPI.begin();
    SPI.beginTransaction(SPISettings(1250000, MSBFIRST, SPI_MODE0)); // clock speed: 1.25MHz, MSB first, SPI mode 0

      // Initialize MCP2515 running at 16MHz with a baudrate of 100kb/s and the masks and filters disabled.
    if(CAN.begin(MCP_ANY, CAN_100KBPS, MCP_16MHZ) == CAN_OK) { 
        Serial.println("SETUP: MCP2515 Initialized Successfully!");
    }
    else Serial.println("SETUP: Error Initializing MCP2515...");

    // Set CAN filter to receive only message with ID 0x5C1
    CAN.init_Mask(0, 0, msgIDSteeringWheelKeys);
    CAN.setMode(MCP_NORMAL);
    pinMode(INTERRUPT_CAN, INPUT);

    // Initialize MCP4151 with 100k
    POTI.setResistance(100000);
    
    Serial.println("SETUP: Startup is complete.");

    // Configure test pin
    pinMode(TEST_PIN, INPUT_PULLUP);
}

void loop() {
    if(!digitalRead(INTERRUPT_CAN)) // If pin 2 is low, read receive buffer
    {
      if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
          // Check if the received CAN ID matches msgIDSteeringWheelKeys
          if (rxId == msgIDSteeringWheelKeys) {
            Serial.print("LOOP: Received CAN ID: 0x");
            Serial.print(rxId, HEX);
            Serial.print(" Data: 0x");
            for(int i = 0; i<len; i++) { // Print each byte of the data
                if(rxBuf[i] < 0x10) // If data byte is less than 0x10, add a leading zero
                {
                Serial.print("0");
                }
                Serial.print(rxBuf[i], HEX);
                Serial.print(" ");
            }
            
            Serial.println();
            unsigned char pressedKey = rxBuf[0];
            keyAction(pressedKey);
          // Only show the "else" message every 100th time
          } else {
              static int nonTargetedCount = 0;
              nonTargetedCount++;
              if (nonTargetedCount % 100 == 0) {
                  // Serial.println("LOOP: Received non-targeted CAN ID, ignored.");
                  nonTargetedCount = 0; // Reset the counter after printing
              }
          }
      } else {
          Serial.println("LOOP: Error reading CAN message.");
      }
    }

    // Test mode via HW Pin
    if(digitalRead(TEST_PIN) == LOW) {
        Serial.println("LOOP: Test input pulled LOW");
        // Configure CAN controller to loopback mode
        CAN.setMode(MCP_LOOPBACK);
        // Send the value 0x1A on CAN ID 0x5C1
        unsigned char buf[1] = {CAN_VOL_UP};
        if (CAN.sendMsgBuf(msgIDSteeringWheelKeys, 0, 1, buf) == CAN_OK) {
            Serial.println("LOOP: Test CAN message sent successfully.");
        } else {
            Serial.println("LOOP: Failed to send test CAN message.");
        }
        delay(500);
    }
    
    // Test mode via serial interface
    if (Serial.available() > 0) {
        char inputChar = Serial.read();

        uint8_t simulatedKey;
        switch(inputChar) {
            case 'u': simulatedKey = CAN_VOL_UP; break;
            case 'd': simulatedKey = CAN_VOL_DOWN; break;
            case 'h': simulatedKey = CAN_HANG_UP; break;
            case 'j': simulatedKey = CAN_JOKER; break;
            case 'n': simulatedKey = CAN_NOKEY; break;
            default: break;
        }
        keyAction(simulatedKey);
    }
}
