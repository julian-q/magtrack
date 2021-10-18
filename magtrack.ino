/* magtrack.ino
 * Hogan Lab
 * by Julian Quevedo
 * -----------------------------
 * On each loop cycle, this sketch measures the magnetic field along the
 * X, Y, and Z axes as well as the current position of the device along 
 * the nylon cable. The data is stored in 3 parallel arrays in a CSV
 * format and is written to an inserted micro SD card.
 */

#include <stdio.h>
#include <stdlib.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <Encoder.h>

// Define pins.
#define CHIP_SELECT 10
#define ENCODER_A 2
#define ENCODER_B 3
#define MAG_TX 4
#define MAG_RX 5

// Tangential distance (cm) traveled between each encoder pulse.
#define TICK_DIST (2 * PI * 1.1 / 800.0)

#define FAT32_MAX_LEN (8 + 1 + 3 + 1)
// Number of bytes in magnetometer output.
#define MAG_BUFFER_LEN 100
#define MAG_NUM_WIDTH 8
#define RECORD_LEN 41

File dataFile;

Encoder encoder(ENCODER_A, ENCODER_B);
SoftwareSerial magSerial(MAG_RX, MAG_TX);

/* FUNCTION: printEncoderChannels
 * ------------------------------
 * This function can be used for debugging encoder wheel input. It prints the
 * logic levels A, B of the encoder output channels so that they can be monitored
 * using the Arduino serial plotter.
 */
void printEncoderChannels() {
  char s[6];
  sprintf(s, "%d, %d", digitalRead(ENCODER_A) + 2, digitalRead(ENCODER_B));
  Serial.println(s);
}

/* FUNCTION: magTerminal
 * ---------------------
 * This function turns the Arduino serial monitor into a direct terminal to
 * the magnetometer. This function should be called in `loop` for debugging
 * the magnetometer. Issue ASCII commands to the device (e.g. "0sd" to send
 * magnetic field data) by typing them into the serial input line and pressing
 * Enter. The magnetometer response message will automatically be written to
 * the terminal screen.
 */
void magTerminal() {
  if (magSerial.available()) {
    Serial.write(magSerial.read());
  }
  if (Serial.available()) {
    magSerial.write(Serial.read());
  }
}

/* FUNCTION: setup
 * ---------------
 * This function initializes each DIO pins and connects to two serial devices:
 * the magnetometer and the SD card module. It also creates the output CSV file
 * where the field-position measurements are stored.
 */
void setup() {
  // Set up pinout.
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  pinMode(MAG_TX, OUTPUT);
  pinMode(MAG_RX, INPUT);
  pinMode(CHIP_SELECT, LOW);
  
  // Open serial communications and wait for port to 
  // open:
  Serial.begin(115200);
  // Wait for serial port to connect. Needed for native 
  // USB port only.
  while (!Serial); 

  magSerial.begin(9600);
  while (!magSerial);
  
  Serial.print("Initializing SD card...");
  if (!SD.begin(CHIP_SELECT)) {
    Serial.println("initialization failed!");
    while (1); // Hangs the program.
  }
  Serial.println("initialization done.");

  // Create a new timestamped file.
  char fileName[FAT32_MAX_LEN];
  unsigned short recordNum;
  EEPROM.get(0, recordNum);
  sprintf(fileName, "mag%05u.csv", recordNum);
  Serial.print("creating ");
  Serial.println(fileName);
  dataFile = SD.open(fileName, FILE_WRITE);
  if (!dataFile) {
    Serial.println("Error opening file!");
    while (1);
  }
  recordNum++;
  EEPROM.put(0, recordNum);

  // wait for dataFile
  delay(1000);
}

// Keep track of the previous X position, so we know when it changes.
double prevX = 0; 
// Buffer where the magnetometer output will be stored.
char magOutput[MAG_BUFFER_LEN];
// Numeric ID for each measurement taken.
int outputNum = 0;

void loop() {
  // The current X position is the number of encoder ticks times the 
  // tangential distance of a single tick.
  double newX = encoder.read() * TICK_DIST;
  // If a new measurement from the magnetometer is available, write a new
  // line to the output CSV file.
  if (magSerial.available()) {
    int numBytes = magSerial.readBytesUntil(4, magOutput, MAG_BUFFER_LEN);
    magOutput[numBytes] = '\0';
    char magData[3][MAG_NUM_WIDTH + 1];
    char *next = magOutput;
    // Parse the magnetometer output for the three numbers corresponding to
    // the magnetic field in the X, Y, and Z directions.
    for (int i = 0; i < 3; i++) {
      next = strstr(next, ":") + 1;
      strncpy(magData[i], next, MAG_NUM_WIDTH);
      magData[i][MAG_NUM_WIDTH] = '\0';
    }
    // Create a new record for the CSV file with the following format:
    //     <measurement number>, <x position>, <m_x>, <m_y>, <m_z>
    char newRecord[RECORD_LEN];
    sprintf(
      newRecord, 
      "%d, %0+10.4f, %s, %s, %s", 
      outputNum,
      newX, 
      magData[0], 
      magData[1], 
      magData[2]
    );
    // Write the new record to both the serial monitor and the CSV file.
    Serial.println(newRecord);
    dataFile.println(newRecord);
    // Every 10 measurements, ensure file has been saved.
    if (outputNum % 10 == 0) {
      dataFile.flush();
    }
    outputNum++;
  // If there is no new data to read from the magnetometer, AND magtrack
  // has moved from its previous position, ask the magnetometer to take a new
  // measurement.
  } else if (prevX != newX) {
    magSerial.write("0sd\r");
  }

  prevX = newX;
}