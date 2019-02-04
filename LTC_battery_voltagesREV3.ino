/* LTC_battery_voltagesREV3.ino
  JAL Notes: January 2019
  Currently set up to read voltages on 10 multiple batteries or cells
  Using Wemos D1 Mini esp8266-12f, LTC2945 I2C power monitor, MCP23017E/SP 16 output I2C muliplexer, (2)ULN2803AP 8 output relay drivers.
  EasyEDA schematic shows wiring details.
  Use an isolated 12V battery with a 5V converter to power this project. For isolating signal GROUND from cells under test.
  REV2 added capability [less2] to stop the program if any 2 cell voltage scans have any cell less than or equal to 0.9 volts.
  REV3 program goes through 6 scan cycles and then halts if both 60 reads and no 2 cells are < 0.9 Volts.
*/

#include <Wire.h>


//define
#define MPLXR (0x20)
//#define LTCADDR B1101111  // (7bit address) DE (0x6F) See Table 1 page 21 of data sheet, Both address pins ADR0, ADR1 grounded
#define LTCADDR (0X6F)
byte ADCvinMSB, ADCvinLSB;
unsigned int ADCvin;
float inputVoltage, ADCvoltage;
int MPLout[11] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 0};  // an array of output numbers which select outputs 0-7 on gpa and 0-1 on gpb
//extra "0" after the second "2" in the array is so you can ask for the 11th value without getting a random memory error.
//for example: array value decimal 1 outputs a HIGH on MPLXR port A GPAO, Array value decimal 128 outputs a HIGH on MPLXR port A GPA7.
//the second decimal 1 and decimal 2 output HIGHS on port B GPBO and GPB1 respectively.
int count = 0;
int less2 = 0; //initialize this variable to zero. If it gets to a count of 2, the program wlll stop executing after elpased time.
int elapcnt = 0; //initialize this variable to zero.  See 'Void elap' when ((elapcnt = 10) + (less2 = 2) = Halt program.

void setup() {
  Serial.begin(9600);
  Serial.println("");
  Wire.begin();

  Wire.beginTransmission(MPLXR);
  Wire.write(0x00); // IODIRA register
  Wire.write(0x00); // set all of port A to outputs
  Wire.endTransmission();

  Wire.beginTransmission(MPLXR);
  Wire.write(0x01); // IODIRB register
  Wire.write(0x00); // set all of port B to outputs
  Wire.endTransmission();

}


void loop() {

  if (count < 8) {
    Wire.beginTransmission(0x20); //Transmit to MPLXR
    Wire.write(0x12); // write to MPLXR port A GPIOA
    Wire.write(MPLout[count]); // energize an output pin on MPLX port A that was indexed by "count" in array MPLout
    Wire.endTransmission();
  }
  else if (count > 7) {
    Wire.beginTransmission(0x20); //Transmit to MPLXR
    Wire.write(0x13); // write to MPLXR port B GPIOB
    Wire.write(MPLout[count]); // energize an output pin on MPLX port B that was indexed by "count" in array MPLout
    Wire.endTransmission();
  }
  delay(250);  //give voltage selected relay time to energize and ADC voltage input reading from battery/cell to settle out
  // The GPA/GPB output pin and the ouput selection relay stays energized until after the ADC reads the voltage input.

  Wire.beginTransmission(LTCADDR);  // Get Input Voltage Up To 80V
  Wire.write(0x1E); //Write 4 bytes address to select Register (0x1E)VinMSB and increment register VinLSB(0x1F)see page 22 of data sheet
  Wire.endTransmission(false);
  Wire.requestFrom(LTCADDR, 2, true); //Master requesting reading 2 bytes from slave device
  delay(1);
  ADCvinMSB = Wire.read(); //Read VinMSB from I2C bus
  ADCvinLSB = Wire.read(); //Read VinLSB from I2C bus
  //Serial.println(ADCvinMSB);
  //Serial.println(ADCvinLSB);
  ADCvin = ((unsigned int)(ADCvinMSB) << 4) + ((ADCvinLSB >> 4) & 0x0F);  //formats into 12bit integer
  inputVoltage = ADCvin * 0.025; //25mV resolution for 12 bit ADC (Range -1 to 100V, 80V max.) 101V/4096Counts = .0246V

  //Next five lines of code allows time for any relay selected to de-energize. Prevent a relay race condition.
  Wire.beginTransmission(0x20); //Transmit to MPLXR
  Wire.write(0x12); // write to MPLXR port A GPIOA
  Wire.write(0); // Write a zero to the MLPXR output pin register that will make all output pins on GPIOA go to a "LOW" state.
  Wire.endTransmission();
  Wire.beginTransmission(0x20); //Transmit to MPLXR
  Wire.write(0x13); // write to MPLXR port B GPIOB
  Wire.write(0); // Write a zero to the MLPXR output pin register that will make all output pins on GPIOB go to a "LOW" state.
  Wire.endTransmission();
  delay(250);

  if (count == 0) {
    Serial.print("Vin: ");
  }
  Serial.print(inputVoltage, 2);  //print inputVoltage 2 decimal places
  Serial.print(","); //comma delimit values in case you want to import into a spreadsheet
  if (count == 9) {
    Serial.println("Volts");
  }

  //Serial.print("count ");
  //Serial.println(count);
  count++;  //increment the array MPLout "count"

  // if (count > 9) {  //If count = 10, add delay for 60 seconds before next set of readings.
  //   delay(60000UL); //Delay of more than 30000(30 seconds) requires the use of an unsigned long integer. So 60 seconds = 60000UL.
  // }

  // if any of the 10 input volts are less than or equal to 0.9 volts, increment counter less2.
  if ((inputVoltage, 2) <= 0.9)  {
    less2 ++;  // increment the less2 counter by 1.
  }

  // if the less2 counter is great than 1, call the halt loop to stop the program.
  // if (less2 > 1)  {
  //   halt();
  // }

  elapcnt ++;  //increment the elapcnt counter by one.

  // elapcnt is equal to the total number of cells read since the program was initialized
  if ((elapcnt > 59) && (less2 > 1)) {   // if elapcnt > 59 and less2 > 1 call 'void elap' to halt the program.
    elap();
  }

  if (count > 9) {   //If count = 10, set count back to 0.  Restart the reading sequence.
    count = 0;
  }

  if (count == 0) {  //If count = 0, add delay before taking next set of readings.
    int val = analogRead(A0); //100k 20 turn potentiometer resistance divider, 0-3.3vin = 0-1024 counts out
    int td = 360;
    unsigned long tde = (td * val);
    delay (tde);  //Approximately 3 seconds to 360 seconds delay by turning potentiometer end to end
  }

}

// This void halt is an endless loop, which stops the rest of the program.  Press reset button on Wemos to restart the program.
//void halt()  {
// while (1) {
//  yield();  // "yield" stops the Wemos from giving a Watch Dog Timer soft reset due to not servicing the void loop for x seconds
//  }

// This void elap is an endless loop, which stops the rest of the program.  Press reset button on Wemos to restart the program.
void elap()  {
  while (1) {
    yield();   // "yield" stops the Wemos from giving a Watch Dog Timer soft reset due to not servicing the void loop for x seconds
  }

}
