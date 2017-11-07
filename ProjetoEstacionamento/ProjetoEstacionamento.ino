#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <LiquidCrystal.h>

#define RS 12
#define EN 11
#define D4 5
#define D5 4
#define D6 3
#define D7 2

#define CONNECTED 0x00
#define DISCONNECTED 0x01

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// Update these with values suitable for your network.
byte mac[] = {0xDE, 0xED, 0xBA, 0xFE, 0xF1, 0xff};

#define NUM_PISCADAS 10

int mqttMachineState;
int blinkLed = LOW;
int contador = NUM_PISCADAS;
unsigned long tempo = NUM_PISCADAS;
unsigned long tempoinicial = 0;
int blinkLedStatus = LOW;

#define TOTAL_VAGAS 1

unsigned int vagas[TOTAL_VAGAS];

int mqttUpdate = 0;

void callback(char *topic, byte *payload, unsigned int length);

EthernetClient ethClient;

// Dados do MQTT Cloud
PubSubClient client("m10.cloudmqtt.com", 16367, callback, ethClient);

void callback(char *topic, byte *payload, unsigned int length)
{
  String sTopic = String(topic); 
  if(sTopic == "senai-code-xp/vagas/kit36"){
    if((char)payload[0] == '1'){
      vagas[0] = 1;
      mqttUpdate |= 1;
    }
    else if((char)payload[0] == '0'){
      vagas[0] = 0;
      mqttUpdate |= 1;
    }
  }
  
  blinkLed = HIGH;
  tempoinicial = millis();
  contador += NUM_PISCADAS;
}

void setup() {
  
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  for(int i = 0; i < TOTAL_VAGAS; i++){
    vagas[i] = 1;
  }

  Ethernet.begin(mac);

  digitalWrite(STATUSLEDYELLOW, ON);
  if (client.connect("Magal", "coiktbwj", "zAhaklL2atGf"))
  {
    // Envia uma mensagem para o cloud no topic portao
    //client.publish("portao", 65);

    // Conecta no topic para receber mensagens
    client.subscribe("senai-code-xp/vagas/*");
    
    mqttMachineState = CONNECTED;
  }else{
    mqttMachineState = DISCONNECTED;
  }
  digitalWrite(STATUSLEDYELLOW, OFF);
}

void loop() {
  // put your main code here, to run repeatedly:

}
