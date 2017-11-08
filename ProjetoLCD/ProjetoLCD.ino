
#include <LiquidCrystal.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xF1, 0x35 }; // Endereço MAC utilizado na placa Ethernet
const int rs = 9, en = 8, d4 = 6, d5 = 5, d6 = 4, d7 = 3; // Pinos de conexão do LCD
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int TOTAL_VAGAS = 13; // Total de vagas do estacionamento
#define POSICAO_TEXTO1 10 // Posição de escrita do número de vagas livres/ocupadas
#define POSICAO_TEXTO2 10

#define STATUSLEDRED A5 // O led vermelho de status da conexão MQTT está no pino A5
#define STATUSLEDGREEN A4 // O led verde de status da conexão MQTT está no pino A4
#define STATUSLEDYELLOW A3 // O led azul de status da conexão MQTT está no pino A3

#define APAGALCD 7 // Pino que controla o brilho do LCD

#define NUM_PISCADAS 10 // Define quantas vezes o led de sinalização de comunicação MQTT irá piscar para cada mensagem

int blinkLed = LOW;
int contador = NUM_PISCADAS;
unsigned long tempo = NUM_PISCADAS;
unsigned long tempoinicial = 0;
int blinkLedStatus = LOW;

#define CONNECTED 0x00
#define DISCONNECTED 0x01
int mqttStateMachine;
#define RECONNECT_TIME 5000 // Intervalo de tempo entre tentativas de reconexão ao servidor MQTT
unsigned long lastConnectAttempt = 0;

int qntVagasLivres = TOTAL_VAGAS;
int qntVagasOcupadas = 0;

int vagas[TOTAL_VAGAS]; // Nesse vetor é registrado o status de cada vaga do estacionamento, vagas[i-1]=1 indica que vaga a vaga i está livre, 0 indica que a vaga i está ocupada

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
  // Calcula o tamanho da string com o nome do tópico MQTT
 int topicSize = 0;
 for(int i = 0;topic[i]!=0; i++)
    topicSize = i + 1;

// Converte os caracteres que indicam o número da vaga em inteiro
  int i = 20;
  int valor = 0;
 while(i < topicSize){
    valor *= 10;
    valor += (int)topic[i]-48;
    i++;
 }
 valor--;

// atualiza o status da vaga i
 if(valor<TOTAL_VAGAS){
  vagas[valor] = (int)payload[0]-48;
  blinkLed = HIGH;
  tempoinicial = millis();
  contador += NUM_PISCADAS;
 }
}

void setup() {

  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("OCUPADAS: ");
  lcd.setCursor(0, 1);
  lcd.print("LIVRES: ");
  
  pinMode(STATUSLEDRED, OUTPUT);
  pinMode(STATUSLEDGREEN, OUTPUT);
  pinMode(STATUSLEDYELLOW, OUTPUT);
  pinMode(APAGALCD, OUTPUT);
  digitalWrite(STATUSLEDRED, HIGH);
  digitalWrite(STATUSLEDGREEN, LOW);
  digitalWrite(STATUSLEDYELLOW, LOW);
  digitalWrite(APAGALCD, LOW);
  
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    vagas[i] = 1;

  }
  digitalWrite(STATUSLEDYELLOW, HIGH);
  lastConnectAttempt = millis();
  Ethernet.begin(mac);
  if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD)) {
   
    // Conecta no topic para receber mensagens
    client.subscribe("senai-code-xp/vagas/#");

    mqttStateMachine = CONNECTED;
    digitalWrite(STATUSLEDGREEN, HIGH);
    digitalWrite(STATUSLEDRED, LOW);
  } else {
    

    mqttStateMachine = DISCONNECTED;
    digitalWrite(STATUSLEDGREEN, LOW);
    digitalWrite(STATUSLEDRED, HIGH);
  }

}

void loop() {
  client.loop();
  int aux = qntVagasLivres;
  qntVagasLivres = atualizaVagasLivres();
  qntVagasOcupadas = atualizaVagasOcupadas();

  if(aux != qntVagasLivres){
    refreshLCD(qntVagasLivres, qntVagasOcupadas);
  }
  
  switch (mqttStateMachine) {
    case CONNECTED: // Caso o estado atual seja conectado, testa se o servidor continua online
      if (!client.connected()) {
        mqttStateMachine = DISCONNECTED; // Caso o servidor esteja offline, muda o estado da máquina para desconectado
        digitalWrite(STATUSLEDGREEN, LOW); // Apaga o led verde
        digitalWrite(STATUSLEDRED, HIGH); // Acende o led vermelho
        lastConnectAttempt = 0;
      }
      break;
    case DISCONNECTED:
      if (millis() - lastConnectAttempt > RECONNECT_TIME) { // Caso o estado atual seja desconectado e o intervalo entre conexões tenha passado, tenta reconectar
        digitalWrite(STATUSLEDYELLOW, HIGH);
        lastConnectAttempt = millis(); // Registra a hora da tentativa de reconexão
        if (client.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD)) {
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
  //delay(1000);

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

void refreshLCD(int livres, int ocupadas){
  if(livres > 0){
    
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print("OCUPADAS: ");
    lcd.setCursor(0, 1);
    lcd.print("LIVRES: ");
    
    lcd.setCursor(POSICAO_TEXTO1, 0);
    lcd.print("  ");
    lcd.setCursor(POSICAO_TEXTO1, 0);
    lcd.print(qntVagasOcupadas);

    lcd.setCursor(POSICAO_TEXTO2, 1);
    lcd.print("  ");
    lcd.setCursor(POSICAO_TEXTO2, 1);
    lcd.print(qntVagasLivres);
  }
  else{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("NAO HA VAGAS");
  }
}

