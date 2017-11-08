//#define OFFLINE 0 // Macro para retirar o código de conexão com o MQTT (apenas para testes)
#ifndef OFFLINE
  #include <SPI.h>
  #include <UIPEthernet.h>
  #include <PubSubClient.h>
#endif

#include <Ultrasonic.h>

#define STATUSLEDRED 6 // O led vermelho de status da conexão MQTT está no pino 7
#define STATUSLEDGREEN 5 // O led verde de status da conexão MQTT está no pino 9
#define STATUSLEDYELLOW 7 // O led azul de status da conexão MQTT está no pino 8

#define VAGAOCUPADALED 4 // Definição do pino de saída do led que indica que a vaga está ocupada
#define VAGALIVRELED 3 // Definição do pino de saída do led que indica que a vaga está livre

#define pino_trigger 8 // Definição do pino de trigger do sensor ultrassonico
#define pino_echo 9 // Definição do pino de echo do sensor ultrassonico

Ultrasonic ultrasonic(pino_trigger, pino_echo);

// Definição dos estados da máquina que controla a conexão/reconexão com o servidor MQTT
#define CONNECTED 0x00
#define DISCONNECTED 0x01

// Update these with values suitable for your network.
byte mac[] = {0xDE, 0xED, 0xBA, 0xFE, 0xF1, 0xff};

#define NUM_PISCADAS 10 // Define quantas vezes o led de sinalização de comunicação MQTT irá piscar para cada mensagem

int mqttStateMachine;
int blinkLed = LOW;
int contador = NUM_PISCADAS;
unsigned long tempo = NUM_PISCADAS;
unsigned long tempoinicial = 0;
int blinkLedStatus = LOW;

#define RECONNECT_TIME 5000 // Intervalo de tempo entre tentativas de reconexão ao servidor MQTT
unsigned long lastConnectAttempt = 0;

#define DISTANCIA 30
#define HISTERESE 1

#ifndef HISTERESE
  #define MIN_DIST DISTANCIA
  #define MAX_DIST MIN_DIST
#else
  #define MIN_DIST DISTANCIA
  #define MAX_DIST DISTANCIA+10
#endif

#define VAGA_OCUPADA 0x01
#define VAGA_LIVRE 0x02

// Dados do servidor e tópico MQTT
#define MQTT_SERVER "m14.cloudmqtt.com"
#define MQTT_ID "Magal"
#define MQTT_USER "asjjbr"
#define MQTT_PASSWORD "1234"
#define MQTT_PORT 11222
#define MQTT_TOPIC "senai-code-xp/vagas/1"

int vagaStateMachine = VAGA_LIVRE;
char vagaStatus = '1';
int mqttUpdate = 0;

#ifndef OFFLINE
void callback(char *topic, byte *payload, unsigned int length);

EthernetClient ethClient;

// Dados do MQTT Cloud
PubSubClient client(MQTT_SERVER, MQTT_PORT, callback, ethClient);

void callback(char *topic, byte *payload, unsigned int length)
{
  blinkLed = HIGH;
  tempoinicial = millis();
  contador += NUM_PISCADAS;
}
#endif

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

  #ifndef OFFLINE
  Ethernet.begin(mac);
  #endif

  #ifndef OFFLINE
  digitalWrite(STATUSLEDYELLOW, HIGH);
  lastConnectAttempt = millis();
  if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD))
  {
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
  #endif
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
  
  #ifndef OFFLINE
  switch(mqttStateMachine){
    case CONNECTED: // Caso o estado atual seja conectado, testa se o servidor continua online
      if(!client.connected()){
        mqttStateMachine = DISCONNECTED; // Caso o servidor esteja offline, muda o estado da máquina para desconectado
        digitalWrite(STATUSLEDGREEN, LOW); // Apaga o led verde
        digitalWrite(STATUSLEDRED, HIGH); // Acende o led vermelho
        lastConnectAttempt = 0;
      }
      else{
        if(mqttUpdate){ // Se o flag indicando uma mensagem a ser publicada no MQTT estiver setado envia o status da vaga
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
      break;
    case DISCONNECTED:
      Serial.println("Desconectado");
      if(millis() - lastConnectAttempt > RECONNECT_TIME){ // Caso o estado atual seja desconectado e o intervalo entre conexões tenha passado, tenta reconectar
        Serial.println("Tentando conectar");
        digitalWrite(STATUSLEDYELLOW, HIGH);
        lastConnectAttempt = millis(); // Registra a hora da tentativa de reconexão
        if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD)){
          Serial.println("Conectado");
          // Se a conexão foi bem sucedida, assina o topic para receber/enviar mensagens
          client.subscribe(MQTT_TOPIC);
        
          mqttStateMachine = CONNECTED; // Muda o estado da máquina para conectado
          digitalWrite(STATUSLEDGREEN, HIGH); // Acende o led verde
          digitalWrite(STATUSLEDRED, LOW); // Apaga o led vermelho
        }
        digitalWrite(STATUSLEDYELLOW, LOW);
      }
      break;
  }
  #endif
  
  if(blinkLed){ // Rotina para piscar o led sempre que uma mensagem for publicada/recebida
    if(contador<=0){
      blinkLed = LOW;
      contador = NUM_PISCADAS;
      digitalWrite(STATUSLEDYELLOW, blinkLed);
    }
    else{
      if(millis() - tempoinicial > tempo){
        blinkLedStatus = !blinkLedStatus;
        digitalWrite(STATUSLEDYELLOW, blinkLedStatus);
        tempoinicial = millis();
        contador--;
      }
    }
  }
}
