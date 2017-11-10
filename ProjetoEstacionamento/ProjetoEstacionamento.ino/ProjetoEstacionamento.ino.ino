
#include <LiquidCrystal.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>

#include <AsyncDelay.h>

AsyncDelay delay_update;
#define UPDATE_TIME 10000

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xF1, 0xff }; // Endereço MAC utilizado na placa Ethernet
const int rs = 9, en = 8, d4 = 6, d5 = 5, d6 = 4, d7 = 3; // Pinos de conexão do LCD
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int TOTAL_VAGAS = 10; // Total de vagas do estacionamento
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

int qntVagasLivres = 0;
int qntVagasOcupadas = 0;
int totalVagas = 0;

int vagas[TOTAL_VAGAS]; // Nesse vetor é registrado o status de cada vaga do estacionamento, vagas[i-1]=1 indica que vaga a vaga i está livre, 0 indica que a vaga i está ocupada
int linkStatus[TOTAL_VAGAS];
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
    vagas[i] = 0;
    linkStatus[i] = 0;
  }

  client.setServer(server, MQTT_PORT);
  client.setCallback(callback);
  
  digitalWrite(STATUSLEDYELLOW, HIGH);
  lastConnectAttempt = millis();
  Serial.println("Iniciando a Ethernet");
  Ethernet.begin(mac);
  Serial.println("Conectando MQTT...");
  if (client.connect(MQTT_ID)){//, MQTT_USER, MQTT_PASSWORD)) {

   Serial.println("MQTT conectado");
    // Conecta no topic para receber mensagens
    client.subscribe(MQTT_TOPIC);

    mqttStateMachine = CONNECTED;
    digitalWrite(STATUSLEDGREEN, HIGH);
    digitalWrite(STATUSLEDRED, LOW);
  } else {
    Serial.println("Falha na conexão");

    mqttStateMachine = DISCONNECTED;
    digitalWrite(STATUSLEDGREEN, LOW);
    digitalWrite(STATUSLEDRED, HIGH);
  }
  delay_update.start(UPDATE_TIME, AsyncDelay::MILLIS);
}

void loop() {
  client.loop();
  int aux1 = qntVagasLivres;
  int aux2 = qntVagasOcupadas;
  totalVagas = atualizaTotalVagas();
  qntVagasLivres = atualizaVagasLivres();
  qntVagasOcupadas = atualizaVagasOcupadas(totalVagas);

  if (aux1!=qntVagasLivres || aux2!=qntVagasOcupadas)
    refreshLCD(qntVagasLivres, qntVagasOcupadas);

  if (delay_update.isExpired()){
    lcd.noDisplay();
  }
  else{
    lcd.display();
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
        if (client.connect(MQTT_ID)){
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
    aux += (vagas[i]*linkStatus[i]);
  }
  //if(aux!=qntVagasLivres){
  //Serial.print("Livres: ");
  //Serial.println(aux);}
  return aux;
}

int atualizaVagasOcupadas(int total) {
  int aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += (vagas[i]*linkStatus[i]);
  }
  aux = total - aux;
  //if(aux!=qntVagasOcupadas){
  //Serial.print("Ocupadas: ");
  //Serial.println(aux);}
  return aux;
}

int atualizaTotalVagas(){
  int aux = 0;
  for (int i = 0; i < TOTAL_VAGAS; i++) {
    aux += linkStatus[i];
  }
  //if(aux!=totalVagas){
  //Serial.print("Total: ");
  //Serial.println(aux);}
  return aux;
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

