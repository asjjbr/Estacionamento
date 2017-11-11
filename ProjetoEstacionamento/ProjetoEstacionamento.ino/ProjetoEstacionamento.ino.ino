#include <LiquidCrystal.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>

#include <AsyncDelay.h>

AsyncDelay delay_update;
#define UPDATE_TIME 10000

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xF1, 0xff }; // Endereço MAC utilizado na placa Ethernet
const byte rs = 9, en = 8, d4 = 6, d5 = 5, d6 = 4, d7 = 3; // Pinos de conexão do LCD
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const byte TOTAL_VAGAS = 100; // Total de vagas do estacionamento
#define POSICAO_TEXTO1 10 // Posição de escrita do número de vagas livres/ocupadas
#define POSICAO_TEXTO2 10

#define STATUSLEDRED A5 // O led vermelho de status da conexão MQTT está no pino A5
#define STATUSLEDGREEN A4 // O led verde de status da conexão MQTT está no pino A4
#define STATUSLEDYELLOW A3 // O led azul de status da conexão MQTT está no pino A3

#define APAGALCD 7 // Pino que controla o brilho do LCD
#define LCDBUTTON A2

#define NUM_PISCADAS 10 // Define quantas vezes o led de sinalização de comunicação MQTT irá piscar para cada mensagem

byte blinkLed = LOW;
byte contador = NUM_PISCADAS;
AsyncDelay delay_blink;
byte blinkLedStatus = LOW;
#define BLINK_TIME 100

#define CONNECTED 0x00
#define DISCONNECTED 0x01
byte mqttStateMachine;
#define RECONNECT_TIME 5000 // Intervalo de tempo entre tentativas de reconexão ao servidor MQTT
AsyncDelay delay_reconnect;

byte qntVagasLivres = 0;
byte qntVagasOcupadas = 0;
byte totalVagas = 0;

byte vagas[TOTAL_VAGAS]; // Nesse vetor é registrado o status de cada vaga do estacionamento, vagas[i-1]=1 indica que vaga a vaga i está livre, 0 indica que a vaga i está ocupada
byte linkStatus[TOTAL_VAGAS];
void callback(char *topic, byte *payload, unsigned int length);

EthernetClient ethClient;

// Dados do MQTT Cloud
IPAddress server(192,168,3,186);
#define MQTT_ID "kit36"
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_PORT 1883
#define MQTT_TOPIC "vagas/#"

//PubSubClient client(myIP, MQTT_PORT, callback, ethClient);
PubSubClient client(ethClient);
void callback(char *topic, byte *payload, unsigned int length)
{
  delay_update.expire();
  delay_update.repeat();
  // Calcula o tamanho da string com o nome do tópico MQTT
  int topicSize = 0;
  for(int i = 0;topic[i]!=0; i++)
    topicSize = i + 1;
  
  int valor = 0;
  if(topicSize<=8){
  // Converte os caracteres que indicam o número da vaga em inteiro
  int i = 6;
  valor = 0;
  while(i < topicSize){
    valor *= 10;
    unsigned int aux = (int)topic[i]-48;
    if (aux>9){
      valor = TOTAL_VAGAS+50;
      break;
    }
    valor += (int)topic[i]-48;
    i++;
  }
  valor--;
  }
  else{
    valor = TOTAL_VAGAS;
  }
  // atualiza o status da vaga i
  if(valor<TOTAL_VAGAS){
    if(length==1){
      vagas[valor] = (int)payload[0]-48;
      linkStatus[valor] = 1;
    }
    else{
      if(length==0){
        vagas[valor] = 0;
        linkStatus[valor] = 0;
      }
    }
    blinkLed = HIGH;
    delay_blink.expire();
    delay_blink.repeat();
    contador += NUM_PISCADAS;
  }
}

void setup() {

  //Serial.begin(9600);

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
  pinMode(LCDBUTTON, INPUT);
  digitalWrite(STATUSLEDRED, HIGH);
  digitalWrite(STATUSLEDGREEN, LOW);
  digitalWrite(STATUSLEDYELLOW, LOW);
  digitalWrite(APAGALCD, LOW);
  
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    vagas[i] = 0;
    linkStatus[i] = 0;
  }

  client.setServer(server, MQTT_PORT);
  client.setCallback(callback);
  
  digitalWrite(STATUSLEDYELLOW, HIGH);
  Ethernet.begin(mac);
  if (client.connect(MQTT_ID)){//, MQTT_USER, MQTT_PASSWORD)) {

    // Conecta no topic para receber mensagens
    client.subscribe(MQTT_TOPIC);

    mqttStateMachine = CONNECTED;
    digitalWrite(STATUSLEDGREEN, HIGH);
    digitalWrite(STATUSLEDRED, LOW);
  } else {
    
    mqttStateMachine = DISCONNECTED;
    digitalWrite(STATUSLEDGREEN, LOW);
    digitalWrite(STATUSLEDRED, HIGH);
  }
  delay_update.start(UPDATE_TIME, AsyncDelay::MILLIS);
  delay_blink.start(BLINK_TIME, AsyncDelay::MILLIS);
  delay_reconnect.start(RECONNECT_TIME, AsyncDelay::MILLIS);
}

void loop() {
  client.loop();
  byte aux1 = qntVagasLivres;
  byte aux2 = qntVagasOcupadas;
  totalVagas = atualizaTotalVagas();
  qntVagasLivres = atualizaVagasLivres();
  qntVagasOcupadas = atualizaVagasOcupadas(totalVagas);

  if (aux1!=qntVagasLivres || aux2!=qntVagasOcupadas)
    refreshLCD(qntVagasLivres, qntVagasOcupadas);

  if (digitalRead(LCDBUTTON) == HIGH){
    delay_update.expire();
    delay_update.repeat();
  }
  if (delay_update.isExpired()){
    lcd.noDisplay();
    digitalWrite(APAGALCD, HIGH);
  }
  else{
    lcd.display();
    digitalWrite(APAGALCD, LOW);
  }
 
  switch (mqttStateMachine) {
    case CONNECTED: // Caso o estado atual seja conectado, testa se o servidor continua online
      if (!client.connected()) {
        mqttStateMachine = DISCONNECTED; // Caso o servidor esteja offline, muda o estado da máquina para desconectado
        digitalWrite(STATUSLEDGREEN, LOW); // Apaga o led verde
        digitalWrite(STATUSLEDRED, HIGH); // Acende o led vermelho
        delay_reconnect.expire();
        delay_reconnect.repeat();
      }
      break;
    case DISCONNECTED:
      if (delay_reconnect.isExpired()) { // Caso o estado atual seja desconectado e o intervalo entre conexões tenha passado, tenta reconectar
        digitalWrite(STATUSLEDYELLOW, HIGH);
        if (client.connect(MQTT_ID)){
          // Se a conexão foi bem sucedida, assina o topic para receber/enviar mensagens
          client.subscribe(MQTT_TOPIC);

          mqttStateMachine = CONNECTED; // Muda o estado da máquina para conectado
          digitalWrite(STATUSLEDGREEN, HIGH); // Acende o led verde
          digitalWrite(STATUSLEDRED, LOW); // Apaga o led vermelho
        }
        delay_reconnect.expire();
        delay_reconnect.repeat();
        digitalWrite(STATUSLEDYELLOW, LOW);
      }
      break;
  }
  
  if(blinkLed){ // Rotina para piscar o led sempre que uma mensagem for publicada/recebida
    if(contador<=0){
      blinkLed = LOW;
      contador = NUM_PISCADAS;
      digitalWrite(STATUSLEDYELLOW, blinkLed);
    }
    else{
      if(delay_blink.isExpired()){
        blinkLedStatus = !blinkLedStatus;
        digitalWrite(STATUSLEDYELLOW, blinkLedStatus);
        delay_blink.repeat();
        contador--;
      }
    }
  }
}

byte atualizaVagasLivres() {
  byte aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += (vagas[i]*linkStatus[i]);
  }
  //if(aux!=qntVagasLivres){
  //Serial.print("Livres: ");
  //Serial.println(aux);}
  return aux;
}

byte atualizaVagasOcupadas(byte total) {
  byte aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += (vagas[i]*linkStatus[i]);
  }
  aux = total - aux;
  //if(aux!=qntVagasOcupadas){
  //Serial.print("Ocupadas: ");
  //Serial.println(aux);}
  return aux;
}

byte atualizaTotalVagas(){
  byte aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += linkStatus[i];
  }
  //if(aux!=totalVagas){
  //Serial.print("Total: ");
  //Serial.println(aux);}
  return aux;
}

void refreshLCD(byte livres, byte ocupadas){
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
    if(ocupadas>0){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("NAO HA VAGAS");
    }
    else{
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("SEM COMUNICAÇÃO");
    }
  }
}






