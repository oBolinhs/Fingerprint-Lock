#include <Keypad.h>
#include <HardwareSerial.h>
#include <sfm.hpp>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>

#define SFM_RX 16
#define SFM_TX 17
#define SFM_IRQ 4
#define SFM_VCC 15
#define RST_PIN 22  // Reset pin
#define SS_PIN 5

//setup do wifi
const char* ssid = "Burger King - Guest";
const char* password = "VinhoVerde";

//configuração do mqtt: server e tópicos
const char* mqtt_server = "broker.emqx.io";
const char* topicState = "bernas/door/state";
const char* authAdd = "bernas/door/addauth";

//configuração de ligação à database
#define API_KEY "AIzaSyAoHBkVgOBatAURRIdDT-zaduk0hHYrxLs"
#define DATABASE_URL "smartlock-44b7d-default-rtdb.europe-west1.firebasedatabase.app"

#define USER_EMAIL "esp32@gmail.com"
#define USER_PASSWORD "12345678"


const byte ROWS = 4;  //
const byte COLS = 3;
int inputIndex = 0;
bool codeChecked = 0;
const int codeLength = 6;
bool correct_code = 0;
char inputCode[codeLength + 1];

WiFiClient espClient;
PubSubClient client(espClient);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


//Configuração do fp
SFM_Module SFM(SFM_VCC, SFM_IRQ, SFM_RX, SFM_TX);
uint16_t tempUid = 0;

// configuração para o keypad


byte rowPins[ROWS] = { 12, 14, 26, 25 };
byte colPins[COLS] = { 33, 32, 27 };

char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);  //declarar o keypad

char numberCode[] = "146817";  //definir o pin de entrada

//Variáveis de estado
bool fingerPrintUN = 0;  //Desbloqueio via fp
bool keypadCodeUN = 0;   //Desbloqueio via keypad
bool doorUnlocked = 0;   //Porta desbloqueada
bool authenticate = 1;   //Variável de autenticação
bool fingerprintNew = 0;
bool fingerprintDelete = 0;
bool resetCode = 0;

Servo lock;

int lockPin = 19;


//funcao para receber mqtt
void callback(char* topic, byte* payload, unsigned int length)  //Funcao que analisa as mensagens recebidas
{
  String message;
  Serial.println("Message Received");
  //Leitura da mensagem recebida por mqtt
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  //identifica qual é o tópico

  if (strcmp(topic, topicState) == 0)  //caso a mensagem seja sobre o estado da fechadura
  {
    if (message == "true") {
      openLock();
    } else if (message == "false") {
      closeLock();
    }

  } else if (strcmp(topic, authAdd) == 0)  //Caso seja premido o botão para adicionar uma fp
  {
    if (message == "fp")  //Adicionar fingerprint
    {
      fingerprintNew = 1;
      SFM.setRingColor(SFM_RING_PURPLE);
    } else if (message == "kp")  //Reiniciar código
    {
      resetCode = 1;
    } else if (message == "dfp")  //Eliminar
    {
      fingerprintDelete = 1;
      SFM.setRingColor(SFM_RING_YELLOW);
    }
  }
}

void fingerprintRead() {
  SFM.setRingColor(SFM_RING_BLUE);
  Serial.println("Coloca o dedo para verificação...");
  delay(500);
  tempUid = 0;
  SFM.recognition_1vN(tempUid);  //corre a leitura e compara às impressões registadas

  if (fingerprintNew) {
    if (tempUid != 0) {
      Serial.println("Impressão já registada");
      fingerprintNew = 0;
    } else  //registar uma nova fingerprint
    {
      SFM.setRingColor(SFM_RING_PURPLE);
      Serial.println("Coloca o dedo para registar (3x seguidas)...");
      bool success = SFM.register_3c3r_1st();  //precisa de 3 leituras
      Serial.println("AGAIN:");

      for (int i = 0; i < 3; i++) {
        if (success) {
          Serial.println("Impressão lida com sucesso!");
          break;
        } else {
          client.loop();
          success = SFM.register_3c3r_1st();
          Serial.println("AGAIN:");
          delay(100);
        }  //até conseguir registar a fingerprint
      }
      if (success) {
        Serial.println("Impressão lida com sucesso!");
        SFM.setRingColor(SFM_RING_GREEN);
        delay(1500);
      }
      fingerprintNew = 0;
    }
  } else if (fingerprintDelete) {
    SFM.setRingColor(SFM_RING_PURPLE);
    if (tempUid != 0) {
      SFM.deleteUser(tempUid);  //elimina o UID associado
      Serial.println("Impressão eliminada com sucesso");
      tempUid = 0;
      fingerprintDelete = 0;
      delay(1500);  //dá tempo para o utilizador tirar o dedo
    } else {
      Serial.println("Impressão não reconhecida");
      SFM.setRingColor(SFM_RING_RED);
      fingerprintDelete = 0;
      delay(1500);
    }
  } else {
    SFM.setRingColor(SFM_RING_BLUE);
    if (tempUid != 0) {
      Serial.print("Impressão reconhecida! UID: ");
      Serial.println(tempUid);  //indica qual o UID em que está registado
      SFM.setRingColor(SFM_RING_GREEN);
      fingerPrintUN = true;  //desbloqueia via FP
      tempUid = 0;
    } else {
      Serial.println("Impressão não reconhecida");
      SFM.setRingColor(SFM_RING_RED);
      delay(1000);
    }
  }
}

void keypadRead() {
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {  // Para garantir que sao numeros
      if (inputIndex < codeLength) {
        inputCode[inputIndex] = key;
        inputIndex++;
        Serial.print("*");  // Para dar feedback
      }
      if (inputIndex == codeLength) {
        inputCode[inputIndex] = '\0';
        Serial.println();
        if (!resetCode)  //caso não seja para resetar código
        {
          if (strcmp(inputCode, numberCode) == 0)  // se o código estiver correto
          {
            correct_code = 1;
          } else {
            correct_code = 0;
          }
          if (correct_code) {
            keypadCodeUN = true;
          } else {
            Serial.println("Código errado");
          }
        } else {
          strcpy(numberCode, inputCode);  //associar novo código
          if (Firebase.ready()) {
            if (Firebase.RTDB.setString(&fbdo, "/keypadcode", numberCode)) {  //registar o novo código numérido na RTDB
              Serial.println("Number code uploaded");
            } else {
            }
            resetCode = 0;
          }
        }
        inputIndex = 0;
      }


    } else if (key == '#') {  // Limpa o input
      inputIndex = 0;
      Serial.println();
      Serial.println("Input cleared.");
    }
  }
}

void taskFP(void* args)  //threading para ler com fingerprint
{
  while (true)  //corre para sempre
  {
    if (authenticate)  //Só quando está em modo autenticação
    {
      fingerprintRead();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Não usa 100% do CPU
  }
}

void taskKP(void* args)  //threading para ler com keypad
{
  while (true) {
    if (authenticate) {
      keypadRead();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
void openLock() {
  Serial.println("Destrancando");
  //enviar unlocked para a RTDB
  if (Firebase.RTDB.setBool(&fbdo, "/lockState", true)) {
    Serial.println("Logged");
  }
  lock.write(0);  //rodar para a posição de aberto
  Serial.println("Destrancado");

  logTime();
}

void closeLock() {
  Serial.println("Trancando");
  //enviar locked para a RTDB
  if (Firebase.RTDB.setBool(&fbdo, "/lockState", false)) {
    Serial.println("Logged");
  }
  lock.write(90);  //rodar para a posição de fechado
  Serial.println("Trancado");
}

void reconnect() {
  // Caso se perca a conexão
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");


    String clientId = "ESPClient-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("CONNECTED!");

      client.subscribe(topicState);
      client.subscribe(authAdd);

    } else {

      int state = client.state();
      Serial.print("failed, rc=");
      Serial.print(state);
      Serial.print(" ");
      //printStateError(state); // Call our helper
      Serial.println(" try again in 5 seconds");

      // Fast blink to indicate error
      for (int i = 0; i < 10; i++) {
        //digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
        delay(100);
      }
    }
  }
}

void logTime() {
  if (Firebase.ready()) {

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }

    //formatar o tempo para string
    char timeString[50];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

    FirebaseJson json;
    json.set("timestamp", timeString);


    Serial.print("Logging time... ");
    if (Firebase.RTDB.pushJSON(&fbdo, "/accessLogs", &json)) {
      Serial.println("OK!");
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}

void setup() {

  Serial.begin(115200);

  //inicializa o modulo wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado ao WiFi");
  }
  delay(1000);  //para garantir que inicializa o wifi corretamente

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


  lock.attach(lockPin);

  SFM.setRingColor(SFM_RING_BLUE);


  xTaskCreatePinnedToCore(taskFP, "taskFP", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskKP, "taskKP", 10000, NULL, 1, NULL, 1);  //no início inicializa ambas funções de verificação

  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, "/keypadcode")) {

      String tempCode = fbdo.stringData();

      Serial.print("Pin Code: ");

      tempCode.toCharArray(numberCode, codeLength + 1);
      Serial.println(numberCode);
    }
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  if (fingerPrintUN || keypadCodeUN) {
    //desbloqueia a porta

    fingerPrintUN = 0;
    keypadCodeUN = 0;  //reseta ambos os estados para não ficar infinito

    doorUnlocked = 1;  //indica que a porta foi desbloqueada

    authenticate = 0;
    delay(1000);
  }
  if (doorUnlocked)  //quando a porta é desbloqueada, independentemente do método
  {
    openLock();
    Serial.println("Door unlocked");

    Serial.println("Welcome");
    doorUnlocked = 0;
    authenticate = 1;
  }
}
