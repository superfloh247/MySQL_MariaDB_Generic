/*********************************************************************************************************************************
  Reboot_WiFiNINA.ino

  Library for communicating with a MySQL or MariaDB Server

  Based on and modified from Dr. Charles A. Bell's MySQL_Connector_Arduino Library https://github.com/ChuckBell/MySQL_Connector_Arduino
  to support nRF52, SAMD21/SAMD51, SAM DUE, STM32F/L/H/G/WB/MP1, ESP8266, ESP32, etc. boards using W5x00, ENC28J60, LAM8742A Ethernet,
  WiFiNINA, ESP-AT, built-in ESP8266/ESP32 WiFi.

  The library provides simple and easy Client interface to MySQL or MariaDB Server.

  Built by Khoi Hoang https://github.com/khoih-prog/MySQL_MariaDB_Generic
  Licensed under MIT license
  Version: 1.0.2

  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  1.0.0   K Hoang      13/08/2020 Initial coding/porting to support nRF52, SAM DUE and SAMD21/SAMD51 boards using W5x00 Ethernet
                                  (Ethernet, EthernetLarge, Ethernet2, Ethernet3 library), WiFiNINA and ESP8266/ESP32-AT shields
  1.0.1   K Hoang      18/08/2020 Add support to Ethernet ENC28J60. Fix bug, optimize code.
  1.0.2   K Hoang      20/08/2020 Fix crashing bug when timeout. Make code more error-proof. Drop support to ESP8266_AT_Webserver.
 **********************************************************************************************************************************/
 
/*
  MySQL Connector/Arduino Example : reboot if connection lost

  This example demonstrates how to reboot an Arduino if connection to the
  server is lost for a period of time.

  For more information and documentation, visit the wiki:
  https://github.com/ChuckBell/MySQL_Connector_Arduino/wiki.

  INSTRUCTIONS FOR USE

  1) Change the address of the server to the IP address of the MySQL server
  2) Change the user and password to a valid MySQL user and password
  3) Connect a USB cable to your Arduino
  4) Select the correct board and port
  5) Compile and upload the sketch to your Arduino
  6) Once uploaded, open Serial Monitor (use 115200 speed) and observe

  To test the reboot, unplug your Ethernet cable once you see "disconnected"
  then wait for the timeout. Once the Arduino reboots, plug the cable in again
  and you should see the query processing resume.

  Created by: Dr. Charles A. Bell
*/

#include "defines.h"
#include "Credentials.h"

#include <MySQL_Generic_WiFiNINA.h>

IPAddress server_addr(192, 168, 2, 112);
uint16_t server_port = 5698;    //3306;

MySQL_Connection conn((Client *)&client);
MySQL_Query query = MySQL_Query(&conn);

int status = WL_IDLE_STATUS;

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setup()
{
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect

  Serial.println("\nStarting Reboot_WiFiNINA on " + String(BOARD_NAME));

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();

  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    //delay(10000);
  }

  printWifiStatus();

  // End WiFi section

  Serial.print("Connecting to SQL Server @ ");
  Serial.print(server_addr);
  Serial.println(String(", Port = ") + server_port);
  Serial.println(String("User = ") + user + String(", PW = ") + password);

  if (conn.connect(server_addr, server_port, user, password))
  {
    delay(1000);
  }
  else
    Serial.println("Connection failed.");
}

// Begin reboot code
int num_fails;                      // variable for number of failure attempts
#define MAX_FAILED_CONNECTS     5   // maximum number of failed connects to MySQL

void soft_reset()
{
#if WIFININA_USE_SAMD
#if ( defined(__SAMD51__) || defined(__SAMD51J20A__) || defined(__SAMD51J19A__) || defined(__SAMD51G19A__)  )
  // For SAMD51
  // see Table 17-5 Timeout Period (valid values 0-11)
  WDT->CONFIG.reg = 5;
  WDT->CTRLA.reg = WDT_CTRLA_ENABLE;
  // To check if OK or bit.ENABLE/CLEAR
  while (WDT->SYNCBUSY.bit.WEN == 1);

  // use the WDT watchdog timer to force a system reset.
  WDT->CLEAR.reg = 0x00;
  // To check if OK or bit.ENABLE/CLEAR
  while (WDT->SYNCBUSY.bit.WEN == 1);
#else
  // For SAMD21, etc
  // see Table 17-5 Timeout Period (valid values 0-11)
  WDT->CONFIG.reg = 5;
  WDT->CTRL.reg = WDT_CTRL_ENABLE;
  while (WDT->STATUS.bit.SYNCBUSY == 1);

  // use the WDT watchdog timer to force a system reset.
  WDT->CLEAR.reg = 0x00;
  while (WDT->STATUS.bit.SYNCBUSY == 1);
#endif
#elif WIFININA_USE_NRF52
  //delay(1000);
  // Restart for nRF52
  NVIC_SystemReset();
#elif WIFININA_USE_SAMDUE
  void(*resetFunc)(void) = 0;
  resetFunc();
#elif WIFININA_USE_STM32
  void(*resetFunc)(void) = 0;
  resetFunc();

#elif WIFININA_USE_TEENSY
#if defined(__IMXRT1062__)
  // Teensy 4.1/4.0
  SCB_AIRCR = 0x05FA0004; //write value for restart for Teensy
#else
  void(*resetFunc)(void) = 0;
  resetFunc();
#endif
#endif
}

// End reboot code

void loop()
{
  if (conn.connected())
  {
    Serial.println("Running a query: SHOW DATABASES");
    
    // Execute the query

    // KH, check if valid before fetching
    if ( !query.execute("SHOW DATABASES") )
    {
      Serial.println("Querying error");
      return;
    }
    //////
  
    query.show_results();             // show the results
    query.close();                    // close the cursor
    conn.close();                     // close the connection
    num_fails = 0;                    // reset failures
    
    delay(10000);
  }
  else
  {
    Serial.print("Connecting to SQL Server @ ");
    Serial.print(server_addr);
    Serial.println(String(", Port = ") + server_port);
    Serial.println(String("User = ") + user + String(", PW = ") + password);

    //if (conn.connect(server_addr, server_port, user, password))
    if (conn.connectNonBlocking(server_addr, server_port, user, password) != RESULT_FAIL)
    {
      delay(500);
    }
    else
    {
      num_fails++;
      Serial.println("Connect failed!");

      if (num_fails == MAX_FAILED_CONNECTS)
      {
        Serial.println("Ok, that's it. I'm outta here. Rebooting...");
        delay(2000);
        // Here we tell the Arduino to reboot by redirecting the instruction
        // pointer to the "top" or position 0. This is a soft reset and may
        // not solve all hardware-related lockups.
        soft_reset();
      }
    }
  }

  Serial.println("\nSleeping...");
  Serial.println("==========================================");
}
