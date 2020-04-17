/*
 * Control 8 relay outputs using buttons and MQTT messages. Subscribes to an MQTT broker and
 * awaits commands.
 * 

 * MQTT command format is "output,state". Output 0 is all outputs. Eg:
 * 1,0 (output 1 off)
 * 3,1 (output 3 on)
 * 0,0 (all outputs off)
 * 0,1 (all outputs on)
 */

//#include "config.h"
/*--------------------------- Configuration ------------------------------*/
/* Network config */
#define ENABLE_DHCP                 true   // true/false
#define ENABLE_MAC_ADDRESS_ROM      true   // true/false
static uint8_t mac[] = { 0x00, 0x, 0xBE, 0xEF, 0xFE, 0xED };  // Set if no MAC
static uint8_t ip[] = { 192, 168, 1, 35 }; // Use if DHCP disabled
#define MQTTBROKER          "IPAddress"


#include <PubSubClient.h>  // MQTT client

// General setup
uint64_t chipid;                 // Global to store the unique chip ID of this device


// MQTT setup
const char* mqtt_broker = MQTTBROKER;     // Configure this in "config.h"
char msg[75];              // General purpose  buffer for MQTT messages
char command_topic[50];    // Dynamically generated using the unique chip ID

// Define pins
int output_pin[8] =   {14, 27, 26, 25, 19, 17, 18, 23};
int output_state[8] = { 0,  0,  0,  0,  0,  0,  0,  0};
const int number_of_output = lenghof(output_pin);


//PubSubClient client(espClient);

void setup() {
    
  Serial.begin(115200);
  Serial.println();
  Serial.println("=====================================");
  Serial.println("Starting OutputBoard v1.0");


  // Set up the topics for MQTT. By inserting the unique ID, the result is
  // of the form: "device/dab9616f/command"
  sprintf(command_topic, "device/%08x/command",  chipid);  // For receiving messages
  
  // Report the topics to the serial console and OLED
  Serial.print("Command topic:       ");
  Serial.println(command_topic);

  // Prepare for OTA updates
  setup_ota();

  // Set up the MQTT client
  client.setServer(mqtt_broker, 1883);
  client.setCallback(mqtt_callback);

  // Make sure all the I/O pins are set up the way we need them
  enable_and_reset_all_outputs();

//display Ip address
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    display.drawString(Ethernet.localIP()[thisByte], DEC);
    if( thisByte < 3 )
    {
      box.print(".");
    }
  } 
}


/*
 * Set all the outputs to low
 */
void enable_and_reset_all_outputs()
{
  for(int i = 0; i < number_of_output; i++)
  {
    pinMode(output_pin[i], OUTPUT);
    digitalWrite(output_pin[i], output_state[i]);
    output_state[i] = 0;
  }
}


/*
 * Callback invoked when an MQTT message is received.
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  byte output_number = payload[0] - '0';
  byte output_state = payload[2] - '0';
  Serial.print("Output: ");
  Serial.println(output_number);
  Serial.print("State: ");
  Serial.println(output_state);

  switch(output_state)
  {
    case 0:
      turn_output_off(output_number);
      break;
    case 1:
      turn_output_on(output_number);
      break;
  }
}

/*
 * Set one or all outputs to off
 */
void turn_output_off( int output_number )
{
  char message[25];
  byte output_index = output_number - 1;

  if(output_number == 0)
  {
    for(int i=0; i<number_of_output; i++)
    {
      digitalWrite(output_pin[i], LOW);
      output_state[i] = 0;
    }
    sprintf(message, "Turning OFF all outputs");
  } else if(output_number < number_of_output) {
    digitalWrite(output_pin[output_index], LOW);
    output_state[output_index] = 0;
    sprintf(message, "Turning OFF output %d", output_number);
  }
  
  Serial.println(message);
}

/*
 * Set one or all outputs to on
 */
void turn_output_on( int output_number )
{
  char message[25];
  byte output_index = output_number - 1;
  
  if(output_number == 0)
  {
    for(int i=0; i<number_of_output; i++)
    {
      digitalWrite(output_pin[i], HIGH);
      output_state[output_index] = 1;
    }
    sprintf(message, "Turning ON all outputs");
  } else if(output_number < number_of_output) {
    digitalWrite(output_pin[output_index], HIGH);
    output_state[output_index] = 1;
    sprintf(message, "Turning ON output %d", output_number);
  }
  
  Serial.println(message);
}

/*
 * Repeatedly attempt connection to MQTT broker until we succeed. Or until the heat death of
 * the universe, whichever comes first
 */
void reconnect() {
  
  char mqtt_client_id[20];
  sprintf(mqtt_client_id, "esp32-%08x", chipid);
  
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      sprintf(msg, "Device %s starting up", mqtt_client_id);
      client.publish("events", msg);
      // ... and resubscribe
      client.subscribe(command_topic);
      Serial.print("Subscribing to ");
      Serial.println(command_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*
 * OTA updates from the Arduino IDE
 */
void setup_ota()
{
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with its md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}


/*
 * =========================================================================
 * Main program loop
 */
void loop() {
  // Handle OTA update requests
  ArduinoOTA.handle();

  // Always try to keep the connection to the MQTT broker alive
  if (!client.connected()) {
    reconnect();
  }

  // Handle any MQTT tasks
  client.loop();

}
