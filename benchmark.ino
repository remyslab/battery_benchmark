/*
   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// Libraries
#include <SPI.h>
#include "SdFat.h"

// Variables
/* Minimal threshold voltage, in Volt, to turn on the red LED (not to start the record, see line 110 */
int min_threshold = 3;
/* Resistor value , ajust with your real value (multimeter) */
float real_value_resistor = 16.2;
/* Measure pin */
byte analog_pin = A3;
/* Values buffer */
char values[100];
/* SD chip select pin.  Be sure to disable any other SPI devices such as Enet. */
byte chipSelect = 4;
/* LEDs */
byte RED = 2;
byte GREEN = 3;
/* Raw value */
int raw = 0;
/* Log file base name.  Must be six characters or less.*/
#define FILE_BASE_NAME "Benchm"
/* Interval between data records in milliseconds.
  // The interval must be greater than the maximum SD write latency plus the
  // time to acquire and write data to the SD to avoid overrun errors.
  // Run a bench example frome the SdFat library to check the quality of your SD card.*/
const uint32_t SAMPLE_INTERVAL_MS = 5000;

// Other stuff
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
char fileName[13] = FILE_BASE_NAME "00.csv";
/* Numeric value to volts */
#define ADC_TO_VOLTS(value) ((value / 1023.0) * 5.0)
/* SdFat definition */
SdFat sd;
SdFile file;
/* Time in micros for next data record. */
uint32_t logTime;
/* Error messages stored in flash. */
#define error(msg) sd.errorHalt(F(msg))

// Function to write data header
void writeHeader() {
  file.println(F("Time,Ubat,Ibat,Pbat"));
}

// Function to log the values.
void logData() {

  raw = analogRead(analog_pin);

  /* Voltage calculation */
  float Ubat = ADC_TO_VOLTS(raw);
  /* Intensity calculation through the resistor (mA), I = U / R */
  float Ibat = (Ubat * 1000) / real_value_resistor;
  /* Battery power calculation (mW), P = U * I */
  float Pbat = Ubat * Ibat;

  /* Start to log time*/
  file.print(logTime);

  /* Write ADC data to CSV record */
  file.write(',');
  file.print(Ubat);
  file.write(',');
  file.print(Ibat);
  file.write(',');
  file.println(Pbat);

  /* Print to serial for the real-time plotting */
  Serial.println(Ubat);  

  /* Update LED statut */
  if (raw < min_threshold*204) {
    digitalWrite(RED, HIGH);
    digitalWrite(GREEN, LOW);
  }
  else {
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, HIGH);
  }
}

void setup() {
  /* Pinmode for LEDs */
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);

  /* Serial */
  Serial.begin(9600);

  delay(1000);

  /* Wait for a battery > 1 volt */
  while (analogRead(analog_pin) < 204) {
    digitalWrite(RED, HIGH);
  }

  /* When a battery is connected, GOOO ! */
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, HIGH);

  /* Initialize at the highest speed supported by the board that is
     not over 50 MHz. Try a lower speed if SPI errors occur.*/
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  /* Find a name to create a new file */
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_CREAT | O_WRITE | O_EXCL)) {
    error("file.open");
  }

  Serial.print(F("Logging to : "));
  Serial.println(fileName);

  /* Write header to the file */
  writeHeader();

  /*  */
  logTime = micros() / (1000UL * SAMPLE_INTERVAL_MS) + 1;
  logTime *= 1000UL * SAMPLE_INTERVAL_MS;
}
//------------------------------------------------------------------------------
void loop() {

  /* Time for next record. */
  logTime += 1000UL * SAMPLE_INTERVAL_MS;

  /* Wait for log time. */
  int32_t diff;
  do {
    diff = micros() - logTime;
  } while (diff < 0);

  /* Check for data rate too high. */
  if (diff > 10) {
    error("Missed data record");
  }

  /* Execute the main function */
  logData();

  /* Force data to SD and update the directory entry to avoid data loss. */
  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }
}

