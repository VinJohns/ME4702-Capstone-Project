/*********************

Capstone Spring 2022
Vincent Johnson, Nicolo Mantovanelli, Caleb Niles, Carter Brock, John Anthony D'Alotto



THINGS TO DO
DEFINE SETTINGS FOR THE POTENTIOMETERS
CALIBRATE THE MOISTURE SENSOR
CALIBRATE TDS SENSOR

Improvements: Using an RTC module to keep time? Connecting to internet to sync time?
That way the user has better control over keeping time rather than trying to do that in the code
Switching the low power library to a better one?

**********************/
// Included Libraries
#include <OneWire.h> // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <LowPower.h> // https://github.com/rocketscream/Low-Power
// An alternative library that could be better is https://github.com/n0m1/Sleep_n0m1 because the programmer specifies the time, so I wouldn't have to make a loop


// Define the pins
#define moisture_pin A0
#define temperature_pin A1
#define tds_pin A2
#define moisture_control_pin A4
#define nutrient_control_pin A5

const int water_pump_pin = 5;
const int nutrient_pump_pin = 6;
const int valve_pin = 9;
const int switch_pin = 8;

float slope = 0.4123;
float intercept = -0.01315;
float SOIL_VOLUME = 4.0*17.0*8.0; // This is the approximate volume of soil (in^3)
float WATER_VOLUME = 1.5*22.0*7.0; // This is the approximate volume of water (in^3)

const int NUM_SAMPLES = 21; // Number of samples to take to get a single reading
OneWire oneWire(temperature_pin); // Code taken from https://randomnerdtutorials.com/guide-for-ds18b20-temperature-sensor-with-arduino/
DallasTemperature sensors(&oneWire);

unsigned long initial_time;
unsigned long current_time;

int nutrient_counter = 0; // Counter for nutrient sensing, only happens once a week, which is 14 times through the main loop

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600); // set up a serial connection with the computer (testing purposes only)
  //analogReadResolution(12); // Analog inputs go from 0 to 4095 (0 - 5V), 12 bits so 2^12 integers (doesn't work with Arduino Uno)

  pinMode(water_pump_pin, OUTPUT); // Set the water pump pin to output so we can control the pump
  pinMode(nutrient_pump_pin, OUTPUT); // Set the nutrient pump pin to output so we can control the pump
  pinMode(valve_pin, OUTPUT); // Set the valve pin to output to control pin
  pinMode(switch_pin, INPUT); // Set the switch pin to input
  
}

// Each loop in the main loop is meant to take 12 hours
void loop() {

  initial_time = millis();
  current_time = initial_time + 1;
  int time_counter = 0; // Counter to make sure nutrient/moisture sensing is done only once every 12 hours

  // There are 30 minutes allotted for nutrient sensing/moisture sensing
  while ((current_time - initial_time) < 30*60*1000) {
    if (time_counter == 0) {
      time_counter++;

      sensors.requestTemperatures(); // Get temperature reading from DS18B20 sensor
      float temperature_input = sensors.getTempCByIndex(0);

      if ((nutrient_counter == 0) & (digitalRead(switch_pin) == HIGH)) { // Perform nutrient sensing
    
        digitalWrite(valve_pin, HIGH); // Open the valve
        delay(1000); // Wait 1 sec for the water to drain out of the valve
        digitalWrite(valve_pin, LOW); // Close the valve
        
        // Create runoff by pumping a lot of water
        digitalWrite(water_pump_pin, HIGH); // Turn the pump on
        delay(2*60000); // Delay for 4 minutes?
        digitalWrite(water_pump_pin, LOW); // Turn the pump off
    
        delay(60000); // Wait 1 minute for runoff
        
        float tds_input = get_median_reading(tds_pin); // Read tds sensor
    
        // Adjust the tds sensor based on temperature and convert voltage to ppm: http://www.cqrobot.wiki/index.php/TDS_(Total_Dissolved_Solids)_Meter_Sensor_SKU:_CQRSENTDS01
        float tds_voltage = tds_input * 5.0 / 1023.0; // Convert the tds input to voltage
        float compensation_coefficient = 1.0 + 0.02 * (temperature_input - 25.0); // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
        float compensation_voltage = tds_voltage / compensation_coefficient; // temperature compensation
        // convert voltage value to tds value (ppm)
        float tds_value = (133.42 * compensation_voltage * compensation_voltage * compensation_voltage - 255.86 * compensation_voltage * compensation_voltage + 857.39 * compensation_voltage) * 0.5;

        // NEED TO FIGURE OUT HOW LONG TO RUN THE NUTRIENT PUMP
        int nutrient_setting = analogRead(nutrient_control_pin); // Check the potentiometer for nutrient setting
        float nutrient_amount = 120 + nutrient_setting/1023.0 * 640; // target ppm of nutrients in runoff water (assume nutrient solution is roughly same density as water, ppm is approx mg/L)
        nutrient_amount = nutrient_amount - tds_value; // (ppm)

        nutrient_amount = nutrient_amount / 1000 * WATER_VOLUME / 61.024; // convert water volume to L from in^3 and nutrient amount (ppm) to g, 
        
        if (nutrient_amount <= 0) {
          float nutrient_time = 0;
        }
        else {
          // Assuming a density of 1 g/mL, nutrient_amount is in mL
          float nutrient_time = nutrient_amount / 0.15; // how long to water the plants per day, assuming the drip emitters have an average flow of 1 mL/s?

          // Water for this long
          digitalWrite(nutrient_pump_pin, HIGH); // Turn the pump on
          delay(nutrient_time); // Delay for length of time specified above
          digitalWrite(nutrient_pump_pin, LOW); // Turn the pump off
          //analogWrite(nutrient_pump_pin, 200); // Write to pump (0 - 256) (check if this affects the pump's output)
        }
        

      }
      else { // Perform moisture sensing and watering

        float moisture_setting = analogRead(moisture_control_pin); // Check the potentiometer for moisture setting
    
        float water_amount = 165 + moisture_setting/1023.0 * 735; // mL amount to water per day
        
        float moisture_input = get_median_reading(moisture_pin); // Read the moisture sensor
        float moisture_voltage = moisture_input * 3.0 / 1023.0; // Convert the moisture sensor analog reading to a voltage
        float moisture = 1.0/moisture_voltage * slope + intercept; // Calibrate the moisture sensor https://makersportal.com/blog/2020/5/26/capacitive-soil-moisture-calibration-with-arduino
      
        // Adjust the moisture readings based on the temperature reading, most likely an equation
        // Maybe use an equation from here: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3444127/
        //if (temperature_input > 20 && temperature_input < 30) {
        //  moisture_input += 20;
        //}
    
        float current_moisture = moisture * 1500; // approximate amount of water in soil in cm^3?? or in^3?? depends on calibration?, 1.5 Kg of soil?
        //current_moisture = current_moisture / 61.024 * 1000; // convert to mL from in^3
    
        water_amount = water_amount - current_moisture; // adjust the amount to water based on the current moisture
        if (water_amount <= 0) {
          float water_time = 0;
        }
        else {
          float water_time = water_amount / 6.0; // how long to water the plants per day, assuming the drip emitters have an average flow of 6 mL/s
          
          // Water for this long
          digitalWrite(water_pump_pin, HIGH); // Turn the pump on
          delay(water_time); // Delay for length of time specified above WATER FLOW FROM 7 DRIP EMITTERS IS ABOUT 6-7 mL/s
          digitalWrite(water_pump_pin, LOW); // Turn the pump off
          //analogWrite(water_pump_pin, 200); // Write to pump (0 - 256) (check if this affects the pump's output)
        }
    

        nutrient_counter++; // Update nutrient counter

        if (nutrient_counter == 14) {
          nutrient_counter = 0; // Reset the nutrient counter after 1 week
        }
        
      }

      current_time = millis(); // Update the current time
      
    }
    
    else { // Keep the while loop going until 30 minutes have passed
      delay(100);
      current_time = millis(); // Update the current time
    }

  }

  // For the rest of the 11 hours and 30 minutes, keep the Arduino in sleep mode to reduce power
  // Testing the repeated sleep loop showed that 10 loops took 90 sec, so (11*3600 + 30*60)/90 * 10 = 4600
  for (int i = 0; i < 4600; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }

}



// Take multiple samples and get the median for a single reading, hopefully removes outliers and is more accurate
float get_median_reading(int pin){
  
  int counter = 0;
  int values[NUM_SAMPLES];

  while (counter < NUM_SAMPLES){

    int input = analogRead(pin);
    values[counter] = input;

    counter++;
    delay(50); // delay for 50 millisec between readings
    
  }

  // Bubble sort code taken from http://www.cqrobot.wiki/index.php/TDS_(Total_Dissolved_Solids)_Meter_Sensor_SKU:_CQRSENTDS01
  int val_temp;
  for (int j = 0; j < NUM_SAMPLES - 1; j++)
  {
    for (int i = 0; i < NUM_SAMPLES - j - 1; i++)
    {
      if (values[i] > values[i + 1])
      {
        val_temp = values[i];
        values[i] = values[i + 1];
        values[i + 1] = val_temp;
      }
    }
  }

  float median_value;
  
  // Find median value
  if ((NUM_SAMPLES & 1) > 0) { // If the number of samples is an odd number
    median_value = float(values[(NUM_SAMPLES - 1) / 2]);
  }
  else { // If the number of samples is an even number
    median_value = float(values[NUM_SAMPLES / 2] + values[NUM_SAMPLES / 2 - 1]) / 2.0;
  }
    
  return median_value;
}
