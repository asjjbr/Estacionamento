#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <Ultrasonic.h>

#define STATUSLEDRED 6 // O led vermelho de status da conexão MQTT está no pino 7
#define STATUSLEDGREEN 5 // O led verde de status da conexão MQTT está no pino 9
#define STATUSLEDYELLOW 7 // O led azul de status da conexão MQTT está no pino 8

#define VAGAOCUPADALED 3
#define VAGALIVRELED 2

Ultrasonic ultrasonic(8, 9);

#define CONNECTED 0x00
#define DISCONNECTED 0x01

// Update these with values suitable for your network.
byte mac[] = {0xDE, 0xED, 0xBA, 0xFE, 0xF1, 0xff};

#define NUM_PISCADAS 10

int mqttStateMachine;
int blinkLed = LOW;
int contador = NUM_PISCADAS;
unsigned long tempo = NUM_PISCADAS;
unsigned long tempoinicial = 0;
int blinkLedStatus = LOW;

#define RECONNECT_TIME 5000
unsigned long lastConnectAttempt = 0;

#define DISTANCIA 30
//#define HISTERESE 1

#ifndef HISTERESE
  #define MIN_DIST DISTANCIA
  #define MAX_DIST MIN_DIST
#else
  #define MIN_DIST DISTANCIA
  #define MAX_DIST DISTANCIA+3
#endif

#define VAGA_OCUPADA 0x01
#define VAGA_LIVRE 0x02
#define MQTT_TOPIC "senai-code-xp/vagas/kit36"
int vagaStateMachine = VAGA_LIVRE;
char vagaStatus = '1';
int mqttUpdate = 0;

void callback(char *topic, byte *payload, unsigned int length);

EthernetClient ethClient;

// Dados do MQTT Cloud
PubSubClient client("m10.cloudmqtt.com", 16367, callback, ethClient);

void callback(char *topic, byte *payload, unsigned int length)
{
  blinkLed = HIGH;
  tempoinicial = millis();
  contador += NUM_PISCADAS;
}

void setup() {
  
  Serial.begin(9600);

  pinMode(STATUSLEDRED, OUTPUT);
  pinMode(STATUSLEDGREEN, OUTPUT);
  pinMode(STATUSLEDYELLOW, OUTPUT);
  pinMode(VAGAOCUPADALED, OUTPUT);
  pinMode(VAGALIVRELED, OUTPUT);
  
  digitalWrite(STATUSLEDRED, HIGH);
  digitalWrite(STATUSLEDGREEN, LOW);
  digitalWrite(STATUSLEDYELLOW, LOW);
  digitalWrite(VAGAOCUPADALED, LOW);
  digitalWrite(VAGALIVRELED, HIGH);
  
  Ethernet.begin(mac);

  digitalWrite(STATUSLEDYELLOW, HIGH);
  lastConnectAttempt = millis();
  if (client.connect("Magal", "coiktbwj", "zAhaklL2atGf"))
  {
    // Envia uma mensagem para o cloud no topic portao
    //client.publish("portao", 65);

    // Conecta no topic para receber mensagens
    client.subscribe(MQTT_TOPIC);
    
    mqttStateMachine = CONNECTED;
    digitalWrite(STATUSLEDGREEN, HIGH);
    digitalWrite(STATUSLEDRED, LOW);
  }else{
    mqttStateMachine = DISCONNECTED;
    digitalWrite(STATUSLEDGREEN, LOW);
    digitalWrite(STATUSLEDRED, HIGH);
  }
  digitalWrite(STATUSLEDYELLOW, LOW);
}

void loop() {

  int distancia = ultrasonic.distanceRead();

  switch(vagaStateMachine){
    case VAGA_OCUPADA:
      if(distancia > MAX_DIST){
        mqttUpdate = 1;
        vagaStatus = '1';
        digitalWrite(VAGAOCUPADALED, LOW);
        digitalWrite(VAGALIVRELED, HIGH);
        vagaStateMachine = VAGA_LIVRE;
      }
      break;
    case VAGA_LIVRE:
      if(distancia < MIN_DIST){
        mqttUpdate = 1;
        vagaStatus = '0';
        digitalWrite(VAGAOCUPADALED, HIGH);
        digitalWrite(VAGALIVRELED, LOW);
        vagaStateMachine = VAGA_OCUPADA;
      }
      break;
  }
  
  switch(mqttStateMachine){
    case CONNECTED:
      if(!client.connected()){
        mqttStateMachine = DISCONNECTED;
        digitalWrite(STATUSLEDGREEN, LOW);
        digitalWrite(STATUSLEDRED, HIGH);
        lastConnectAttempt = 0;
      }
      break;
    case DISCONNECTED:
      if(millis() - lastConnectAttempt > RECONNECT_TIME){
        if (client.connect("Magal", "coiktbwj", "zAhaklL2atGf")){
          // Conecta no topic para receber mensagens
          client.subscribe(MQTT_TOPIC);
        
          mqttStateMachine = CONNECTED;
          digitalWrite(STATUSLEDGREEN, HIGH);
          digitalWrite(STATUSLEDRED, LOW);
          lastConnectAttempt = millis();
        }
      }
      break;
  }

  if(blinkLed){
    if(contador<=0){
      blinkLed = LOW;
      contador = NUM_PISCADAS;
      digitalWrite(STATUSLEDYELLOW, blinkLed);
    }
    else{
      if(millis() - tempoinicial > tempo){
        blinkLedStatus = !blinkLedStatus;
        digitalWrite(STATUSLEDYELLOW, blinkLed);
        tempoinicial = millis();
        contador--;
      }
    }
  }
  else
    digitalWrite(STATUSLEDYELLOW, blinkLed);

  if(mqttUpdate){
    boolean flag;
    flag = client.publish(MQTT_TOPIC, vagaStatus, true);
    if(flag){
      blinkLed = HIGH;
      tempoinicial = millis();
      contador += NUM_PISCADAS;
      mqttUpdate = 0;
    }
  }

  
}
