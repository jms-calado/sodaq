# Carelink

## Sodaq SFF R412M board

### Instructions for sodaq

#### First Time Use

  Open Arduino IDE

  In File->Preferences->'Aditional Boards Manager URLs:' paste ```http://downloads.sodaq.net/package_sodaq_samd_index.json``` and click 'ok'

  In Tools->Board->Boards Manager filter for "Sodaq" and install the only package ("SODAQ SAMD Boards by Sodaq") - Version 1.6.19

  In Tools->Manage Librares install the following libraries:

      Sodaq_LSM303AGR (Version 2.0.0)
      Sodaq_nbIOT (Version 2.0.1)
      Sodaq_R4X (Version 1.0.0)
      Sodaq_UBlox_GPS (Version 0.9.5)
      Sodaq_wdt (Version 1.0.2)
      ArduinoJson by Benoit Blanchon (Version 6.10.1)

#### Normal Use

  In Arduino IDE, open the "teste_MQTT_GPS.ino" file, located in the extracted zip

  In ```MQTT_USERNAME    "username"``` change ```username``` to the correct username

  In ```MQTT_KEY         "password"``` change ```password``` to the correct password

  In ```deviceId = "PS99";``` change ```PS99``` to the correct label

  In Tools->Board: scroll down and select "Sodaq SFF"

  In Tools->Port: select the board COM port (e.g. "COM16 (SODAQ SARA)")

  Sketch->Verify/Compile

  Sketch->Upload

  Tools->Serial Monitor (115200 baud)

  The serial monitor should show the board logs and info if successful

  Repeat for other devices
