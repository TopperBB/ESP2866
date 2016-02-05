#include <ESP8266WiFi.h>  // Include the ESP8266 WiFi library. (Works a lot like the Arduino WiFi library.)
#include <ESP8266HTTPClient.h> // Include the ESP8266 wifi http client. 
#include <OneWire.h>
#include <Phant.h>

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "xxxx";   // ... YOUR SSID ....
const char WiFiPSK[] = "xxxx"; // ... YOUR PASSWORD ....

////////////////
// Phant Keys //
////////////////
const char PhantHost[] = "data.sparkfun.com";
const char PublicKey[] = "xxxx"; // Your public key 
const char PrivateKey[] = "xxxx";  // Your private key 

Phant phant("data.sparkfun.com", PublicKey, PrivateKey);


/////////////////////
// One Wire        //
/////////////////////
const int ONE_WIRE_BUS = D6; 
OneWire  ds(ONE_WIRE_BUS);  // on pin 10 (a 4.7K resistor is necessary)



/////////////////
// Post Timing //
/////////////////
const unsigned long postRate = 30000;
unsigned long lastPost = 0;


WiFiClient client; // Create an ESP8266 WiFiClient class to connect 

byte ledStatus = LOW;     

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  
  Serial.begin(115200);
  delay(10); 
  Serial.println("VHS - ESP + DS1820");

  connectWiFi();
}

void loop()
{
  static unsigned long previousMillisTemp = 0 ; 
  unsigned long currentMillisTemp = millis();
  if(currentMillisTemp - previousMillisTemp >= postRate ) {
    previousMillisTemp = currentMillisTemp;   

    // Flip the LED 
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;
    digitalWrite(BUILTIN_LED, ledStatus);

    // Get the temp 
    float celsius = 0.0f; 
    if( ! GetTemp (&celsius ) ){
      Serial.println("Error could not get temp" );
    } else {
      Serial.println("celsius="+ String(celsius) );  

      phant.add("celsius", celsius);
      // Serial.println("----TEST URL-----");
      // Serial.println(phant.url());      


      HTTPClient http;
      http.begin(phant.url()); //HTTP
      Serial.println("[HTTP] GET..." +String(phant.url()) );
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if(httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] GET... code: %d\n", httpCode);

          // file found at server
          if(httpCode == HTTP_CODE_OK) {
              String payload = http.getString();
              Serial.println(payload);
          }
      } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }
}


bool GetTemp (float * celsius ) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  if( celsius == NULL ) {
    return false; 
  }
  
  if ( !ds.search(addr)) {
    ds.reset_search();
    Serial.println("FYI: No more oneWire Sensors found " );
    return false; 
  }
  
  Serial.print("Address: ");
  for( i = 0; i < 8; i++) {
    if( addr[i] < 10 ) { 
      Serial.print("0");
    }
    Serial.print(addr[i], HEX);
  }
  Serial.print(", ");

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return false; 
  }
  // Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      // Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      // Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      // Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return false; 
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  // delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  // Serial.print("  Data = ");
  // Serial.print(present, HEX);
  // Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    // Serial.print(data[i], HEX);
    // Serial.print(" ");
  }
  // Serial.print(" CRC=");
  // Serial.print(OneWire::crc8(data, 8), HEX);
  // Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  *celsius = (float)raw / 16.0;
  Serial.print("  Temperature = ");
  Serial.print(*celsius);
  Serial.println(" Celsius");
  return true; 
}



void connectWiFi()
{
  Serial.print("Connecting to ");
  Serial.println(WiFiSSID);
    
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);
  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);
  
  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(BUILTIN_LED, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;
    
    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
    Serial.print(".");
  }

  // Print the wifi connection details. 
  Serial.println("WiFi connected");
  Serial.println("IP address: "); 
  Serial.println(WiFi.localIP());  
}


