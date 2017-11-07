
#include <LiquidCrystal.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xF1, 0x35 };
const int rs = 9, en = 8, d4 = 6, d5 = 5, d6 = 4, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int TOTAL_VAGAS = 10;
#define POSICAO_TEXTO1 10
#define POSICAO_TEXTO2 10

#define CONNECTED 0x00
#define DISCONNECTED 0x01
int mqttStateMachine;
#define RECONNECT_TIME 5000 // Intervalo de tempo entre tentativas de reconexão ao servidor MQTT
unsigned long lastConnectAttempt = 0;

int qntVagasLivres = TOTAL_VAGAS;
int qntVagasOcupadas = 0;

int vagas[TOTAL_VAGAS];

void callback(char *topic, byte *payload, unsigned int length);

EthernetClient ethClient;

// Dados do MQTT Cloud
#define MQTT_SERVER "m14.cloudmqtt.com"
#define MQTT_ID "Magal"
#define MQTT_USER "asjjbr"
#define MQTT_PASSWORD "1234"
#define MQTT_PORT 11222
#define MQTT_TOPIC "senai-code-xp/vagas/#"

PubSubClient client(MQTT_SERVER, MQTT_PORT, callback, ethClient);
void callback(char *topic, byte *payload, unsigned int length)
{
  //  blinkLed = HIGH;
  //  tempoinicial = millis();
  //  contador += NUM_PISCADAS;
  //Serial.println("callback");
  //for (int i = 0; topic[i] != 0; i++) {
  //  Serial.println(topic[i]);
 // }
 int i = (int)topic[21]-49;
 vagas[i] = (int)payload[0]-48;

}

void setup() {

  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("OCUPADAS: ");
  lcd.setCursor(0, 1);
  lcd.print("LIVRES: ");

  for (int i = 0; i < TOTAL_VAGAS; i++) {
    vagas[i] = 1;

  }
  Ethernet.begin(mac);
  if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD)) {
   
    // Conecta no topic para receber mensagens
    client.subscribe("senai-code-xp/vagas/#");

    mqttStateMachine = CONNECTED;
    //digitalWrite(STATUSLEDGREEN, HIGH);
    //digitalWrite(STATUSLEDRED, LOW);
  } else {
    

    mqttStateMachine = DISCONNECTED;
    //digitalWrite(STATUSLEDGREEN, LOW);
    //digitalWrite(STATUSLEDRED, HIGH);
  }

}

void loop() {
  client.loop();
  qntVagasLivres = atualizaVagasLivres();
  qntVagasOcupadas = atualizaVagasOcupadas();

  lcd.setCursor(POSICAO_TEXTO1, 0);
  lcd.print("  ");
  lcd.setCursor(POSICAO_TEXTO1, 0);
  lcd.print(qntVagasOcupadas);

  lcd.setCursor(POSICAO_TEXTO2, 1);
  lcd.print("  ");
  lcd.setCursor(POSICAO_TEXTO2, 1);
  lcd.print(qntVagasLivres);
 
  switch (mqttStateMachine) {
    case CONNECTED: // Caso o estado atual seja conectado, testa se o servidor continua online
      if (!client.connected()) {
        mqttStateMachine = DISCONNECTED; // Caso o servidor esteja offline, muda o estado da máquina para desconectado
        //digitalWrite(STATUSLEDGREEN, LOW); // Apaga o led verde
        //digitalWrite(STATUSLEDRED, HIGH); // Acende o led vermelho
        lastConnectAttempt = 0;
      }
      else {
//        if (mqttUpdate) { // Se o flag indicando uma mensagem a ser publicada no MQTT estiver setado envia o status da vaga
//          boolean flag;
//          flag = client.publish(MQTT_TOPIC, vagaStatus, true);
//          if (flag) {
//            blinkLed = HIGH;
//            tempoinicial = millis();
//            contador += NUM_PISCADAS;
//            mqttUpdate = 0;
//          }
//        }
      }
      break;
    case DISCONNECTED:
      Serial.println("Desconectado");
      if (millis() - lastConnectAttempt > RECONNECT_TIME) { // Caso o estado atual seja desconectado e o intervalo entre conexões tenha passado, tenta reconectar
        Serial.println("Tentando conectar");
//        digitalWrite(STATUSLEDYELLOW, HIGH);
        lastConnectAttempt = millis(); // Registra a hora da tentativa de reconexão
        if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD)) {
          Serial.println("Conectado");
          // Se a conexão foi bem sucedida, assina o topic para receber/enviar mensagens
          client.subscribe(MQTT_TOPIC);

          mqttStateMachine = CONNECTED; // Muda o estado da máquina para conectado
//          digitalWrite(STATUSLEDGREEN, HIGH); // Acende o led verde
//          digitalWrite(STATUSLEDRED, LOW); // Apaga o led vermelho
        }
//        digitalWrite(STATUSLEDYELLOW, LOW);
      }
      break;
  }
  //delay(1000);
}

int atualizaVagasLivres() {
  int aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += vagas[i];
  }
  return aux;
}

int atualizaVagasOcupadas() {
  int aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += vagas[i];
  }
  return TOTAL_VAGAS - aux;
}



