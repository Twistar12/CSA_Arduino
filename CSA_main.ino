#include <SevSeg.h>
SevSeg sevseg;  // Instance of seven segment object

const int txLed = 2;
const int rxPhoto = A0;
int sensorValue = 0;
const int buttonPin = 3;
const int buzzerPin = 4;
const int tiltPin = A3;
const int ledPin = A4;
const int potPin = A1;
const byte segmentPins[7] = { 7, 8, 9, 10, 11, 12, 13 };  // {a, b, c, d, e, f, g} connect each of them thorugh resistor

// --- Joystick and fan ---
const int JSPin = A5;     // Vx of Joystick
const int motorPin1 = 5;  // Fan Motor output
const int joySW = 6;      // Joystick Pushbutton

// Joystick deadzone
struct deadZone {
  int start = 600;
  int end = 400;
};

// FLETCHER 16 Checksum
struct Fletcher16 {
  byte sum1 = 0;
  byte sum2 = 0;
};

#define CIPHER_KEY 0x23  // Cipher key for encryption
#define START_BYTE 0x70  // ASCII - "p"
#define STOP_BYTE 0x71   // ASCII - "q"
#define THRESHOLD 600    // Threshold set for LDR HIGH reading

#define FRAME_SIZE 9

#define START_BIT_LEVEL HIGH
#define STOP_BIT_1_LEVEL HIGH
#define STOP_BIT_2_LEVEL LOW

#define SAMPLE_SIZE 10

// TO RUN HEAVIER LOGIC IN CERTAIN INTERVALS (NON-BLOCKING)
// WITHOUT IT MAY RESULT IN DATA TEARING LEADING TO CHECKSUM ERRORS
unsigned long lastLogicLoop = 0;
const int logicInterval = 20;

// 0 - stop fan, anything else it spins
int fanSpeed = 0;
int joyX = 0;

int targetFanSpeed = 0;
unsigned long lastUpdate = 0;
const int updateInterval = 20;

static int prevJoySW = HIGH;   // RECORD button state, static
static bool fanState = false;  // Monitor fan state, static

void controlFanSpeed() {
  if (fanState) {
    joyX = analogRead(JSPin);
    static struct deadZone d_zone;  // Static variables

    // Timer so that speed doesnt increase instantly
    if (millis() - lastUpdate > updateInterval) {
      lastUpdate = millis();

      // Alter fan speed
      if (joyX > d_zone.start) {
        targetFanSpeed += 3;
      } else if (joyX < d_zone.end) {
        targetFanSpeed -= 3;
      }
      // If centered do nothing
    targetFanSpeed = constrain(targetFanSpeed, 0, 99);  // Limit fan speed to values [0,99] as per requirements
    }
    fanSpeed = targetFanSpeed;
  }
}

// |--- Fletcher16 Checksum ---|
// Have two sums, both set to zero
// First sum takes the current value -> adds it to itself
// Second sum adds the first sum to itself
// no modulo used her, wrap around happens anyways
void calc_sum(Fletcher16 &c_sum, byte cur_byte) {
  c_sum.sum1 = (c_sum.sum1 + cur_byte) % 255;
  c_sum.sum2 = (c_sum.sum2 + c_sum.sum1) % 255;
}

// Byte txFrame for transmission
// Needs to update values based on input
byte txFrame[FRAME_SIZE];

// Potentiometer averaging to counteract noise in ground line
// Float to prevent truncation
float smoothedPot = 0;

void readTxFrame() {
  // Fan control function
  controlFanSpeed();

  txFrame[0] = START_BYTE;                               // START BYTE
  txFrame[1] = (digitalRead(buttonPin) == LOW ? 1 : 0);  // Button-buzzer (pulldown resistor)
  txFrame[2] = (digitalRead(tiltPin) == LOW ? 1 : 0);    // Tilt Switch-LED

  int rawPot = analogRead(potPin);
  // Weighted averaging - takes in only 30% of new value, 70% of previous value
  // Noise spikes can be reduced by "smoothening the value"
  smoothedPot = (smoothedPot * 0.70) + (rawPot * 0.30);
  int noise_reduction = map((int)smoothedPot, 0, 900, 0, 7);  // upper limit set to lower value because | not reaching max value due to noise
  txFrame[3] = constrain(noise_reduction, 0, 9);              // Potentiometer values from 0-9

  // Joystick Control
  int currentJoySW = digitalRead(joySW);
  if (prevJoySW == HIGH && currentJoySW == LOW) {
    fanState = !fanState;
  }
  prevJoySW = currentJoySW;

  txFrame[4] = fanState;  // joystick button
  txFrame[5] = fanSpeed;  // fanspeed

  // |--- Fletcher16 Checksum ---|
  // Store in final two bytes
  Fletcher16 txCheckSum;  // Intialise checksum struct
  for (int i = 1; i < 6; i++) {
    calc_sum(txCheckSum, txFrame[i]);
  }
  txFrame[6] = txCheckSum.sum1;  // Sum1 of checksum
  txFrame[7] = txCheckSum.sum2;  // Sum2 of checksum

  txFrame[8] = STOP_BYTE;  // STOP BYTE

  // ENCRYPTION USING XOR POSITION INDEXED CIPHER
  for (int i = 1; i < 8; i++) {
    txFrame[i] = txFrame[i] ^ (CIPHER_KEY + i);  // i is the position of current bit
  }
}

// TRANSMITTER VALUES

int txIndex = 0;                     // For accessing bytes in the txFrame
int tx_byte_value = 0;               // Stores byte values from txFrame
int tx_state = 0;                    // Monitor states
unsigned long previousTxMillis = 0;  // To monitor previous instance where the txloop was ran
const long txInterval = 10;          // Interval between each transmission


// Transmission loop (Read-only)
void txLoop() {
  unsigned long currentTxMillis = millis();

  if (currentTxMillis - previousTxMillis >= txInterval) {  // Run after every interval
    previousTxMillis += txInterval;                        // To catch up with delays

    switch (tx_state) {
      case 0:                              // Start bit
        tx_byte_value = txFrame[txIndex];  // Load the byte value
        txIndex++;                         // Index to the byte for the next cycle
        if (txIndex >= FRAME_SIZE) txIndex = 0;

        digitalWrite(txLed, START_BIT_LEVEL);
        tx_state++;  // If overrun, return back to state 1
        break;

      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        if ((tx_byte_value & 0x80) != 0) {  // Data bits being bit-masked
          digitalWrite(txLed, HIGH);
        } else {
          digitalWrite(txLed, LOW);
        }
        tx_byte_value <<= 1;  // Bit shift the byte to the left so that the next bit is in place for bit-masking
        tx_state++;
        break;

      case 9:  // stop bit 1
        digitalWrite(txLed, STOP_BIT_1_LEVEL);
        tx_state++;
        break;

      case 10:  // Stop bit 2
        digitalWrite(txLed, STOP_BIT_2_LEVEL);
        tx_state++;
        break;

      default:  // Idle case
        digitalWrite(txLed, LOW);
        tx_state++;
        if (tx_state > 11) tx_state = 0;  // Idle then resend
        break;
    }
  }
}


// RECEIVER VALUES

// Byte txFrame for receiver
byte rxFrame[FRAME_SIZE];
int rxIndex = 0;
int rx_state = 0;
unsigned long previousRxMillis = 0;
int rx_bits[11] = { 0 };  // Holding 11 bit values sampled 10 times

const int rxInterval = txInterval / SAMPLE_SIZE;

// Receiver Loop (write)
void rxLoop() {
  unsigned long currentRxMillis = millis();

  if (currentRxMillis - previousRxMillis >= rxInterval) {
    previousRxMillis += rxInterval;

    sensorValue = analogRead(rxPhoto);

    switch (rx_state) {
      case 0:
        // Increment ONLY when HIGH signal is received (sync)
        if (sensorValue >= THRESHOLD) {
          rx_bits[0]++;
          rx_state++;  // Increment
        }
        break;

      // 0-109 Sampling bits | 110 for VALIDATION
      case 110:                                                             // END OF CYCLE
        if ((rx_bits[0] >= 4) && (rx_bits[9] >= 4) && (rx_bits[10] < 4)) {  // If bits are matching start and stop bits run
          int rx_byte_value = 0;
          for (int i = 1; i < 9; i++) {  // Data bits from rx_bits[1] to rx_bits[7]
            rx_byte_value <<= 1;         // Shift to the next bit in the byte value
            if (rx_bits[i] >= 4) {
              rx_byte_value |= 0x01;  // Using bitwise OR operator to flip the value to a 1
            }
          }

          // If the byte received is the START BYTE, force Frame index to 0
          // else if the byte received is NOT the STOP_BYTE, must be that it is a data byte
          if (rx_byte_value == START_BYTE) {
            rxIndex = 0;
          } else if (rx_byte_value != STOP_BYTE) {
            // Check if the current byte index is within data limits
            if (rxIndex > 0 && rxIndex < 8) {
              rx_byte_value = rx_byte_value ^ (CIPHER_KEY + rxIndex);  //decrypt
            }
          }
          rxFrame[rxIndex] = rx_byte_value;
          rxIndex++;
          
          // printing Start and Stop byte in HEX, rest end in default DEC (decimal)
          Serial.print("Rx Byte: ");
          if (rx_byte_value == START_BYTE || rx_byte_value == STOP_BYTE) {
            Serial.print("0x");
            Serial.println(rx_byte_value, HEX);
          } else Serial.println(rx_byte_value);

          if (rxIndex >= FRAME_SIZE) rxIndex = 0;

        } else {  // Else, print error with sample rates
          Serial.println("Rx Error");
          Serial.print(rx_bits[0]);
          Serial.print("-");
          Serial.print(rx_bits[9]);
          Serial.print("-");
          Serial.println(rx_bits[10]);
        }

        // Reset rx_bits and rx_state at the end of cycle
        rx_state = 0;
        memset(rx_bits, 0, sizeof(rx_bits));
        break;

      default:
        int bit_index = rx_state / SAMPLE_SIZE;
        // Check for overflow of bit array
        if (bit_index < 11) {
          if (sensorValue >= THRESHOLD) {
            rx_bits[bit_index]++;
          }
        }
        rx_state++;
        break;
    }
  }
}


void runRxFrame() {
  // Check for valid start and stop bytes
  if (rxFrame[0] == START_BYTE && rxFrame[8] == STOP_BYTE) {

    // Fletcher Checksum validation
    Fletcher16 rxCheckSum;
    for (int i = 1; i < 6; i++) {
      calc_sum(rxCheckSum, rxFrame[i]);
    }

    // Compare checksum values with received transmission values, only run if valid
    if (rxCheckSum.sum1 == rxFrame[6] && rxCheckSum.sum2 == rxFrame[7]) {
      // Buzzer - Button
      if (rxFrame[1] == 1) {
        tone(buzzerPin, 500);
      } else {
        noTone(buzzerPin);
      }

      // Tilt switch
      if (rxFrame[2] == 1) {
        digitalWrite(ledPin, HIGH);
      } else {
        digitalWrite(ledPin, LOW);
      }

      // Potentiometer - 1digit seven segment display
      if (rxFrame[3] > 9) {
      } else {
        sevseg.setNumber(rxFrame[3]);
      }

      // Joystick control with button for start/stop
      int rxJoySW = rxFrame[4];
      int rxSpeed = map(rxFrame[5], 0, 99, 0, 255); // Convert back to 0-255 for PWM
      if (rxJoySW) {
        analogWrite(motorPin1, rxSpeed);
      } else {
        analogWrite(motorPin1, 0);
      }
    } else {
      // Serial.println("Failed Checksum"); // use for debugging
    }
  }
}


void setup() {
  pinMode(txLed, OUTPUT);
  pinMode(rxPhoto, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(tiltPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(potPin, INPUT);

  pinMode(JSPin, INPUT);
  pinMode(motorPin1, OUTPUT);
  pinMode(joySW, INPUT_PULLUP);
  //SevSeg automatically assigns pinMode to the segments of the display

  // Part of SevSeg.h to intialise variables for display control
  byte numDigits = 1;
  byte digitPins[] = { A2 };  // Common cathode
  // segmentsPins declared above {a, b, c, d, e, f, g} connect each of them thorugh resistor
  bool resistorsOnSegments = false;      // Where the current limiting resistors are located
  byte hardwareConfig = COMMON_CATHODE;  // Type of 7Seg display
  bool updateWithDelays = false;         // Controls whether blocking (true) / non-blocking (false) updates are carried out
  bool leadingZeros = false;             // Whether to show leading zeroes
  bool disableDecPoint = true;           // Use to disable DP pin, only requires 7 segment pins now

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(80);

  Serial.begin(9600);
}

void loop() {

  if (millis() - lastLogicLoop >= logicInterval) {
    readTxFrame();  // Populate txFrame with values read from input devices
    runRxFrame();   // Populate rxFrame using PhotoResistor (with checksum validation)
    lastLogicLoop = millis();
  }
  txLoop();
  rxLoop();
  sevseg.refreshDisplay();  // To run repeatedly
}

