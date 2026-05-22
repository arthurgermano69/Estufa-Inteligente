#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define RELAY_PIN 5
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_BME680 bme;

const char* ssid     = "";
const char* password = "";

const String BOT_TOKEN = "";
const String CHAT_ID   = "";

unsigned long ultimoEnvio    = 0;
unsigned long ultimoUpdate   = 0;
long          ultimoUpdateId = 0;

const unsigned long INTERVALO_STATUS  = 5UL * 60UL * 1000UL;
const unsigned long INTERVALO_POLLING =        3UL * 1000UL;

bool relayLigado = false;
bool sensorOk    = false;

bool lerSensor(float & temp, float & umi) {
  if (!bme.performReading()) {
    Serial.println("[SENSOR] Falha na leitura.");
    return false;
  }
  temp = bme.temperature;
  umi  = bme.humidity;
  Serial.printf("T: %.1f  H: %.1f\n", temp, umi);
  return true;
}

void enviarTelegram(String mensagem) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  mensagem.replace(" ", "%20");
  mensagem.replace("!", "%21");
  mensagem.replace("°", "%C2%B0");
  String url = "https://api.telegram.org/bot" + BOT_TOKEN +
               "/sendMessage?chat_id=" + CHAT_ID + "&text=" + mensagem;
  http.begin(url);
  http.GET();
  http.end();
}

void verificarComandos() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN +
               "/getUpdates?offset=" + String(ultimoUpdateId + 1) + "&timeout=1";
  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();

    int pos = 0;
    while (true) {
      int idxId = payload.indexOf("\"update_id\":", pos);
      if (idxId == -1) break;

      int start = idxId + 12;
      int end   = payload.indexOf(",", start);
      long uid  = payload.substring(start, end).toInt();
      ultimoUpdateId = uid;

      int idxText = payload.indexOf("\"text\":\"", idxId);
      if (idxText != -1) {
        int tStart   = idxText + 8;
        int tEnd     = payload.indexOf("\"", tStart);
        String texto = payload.substring(tStart, tEnd);
        texto.toLowerCase();
        texto.trim();

        Serial.print("Comando recebido: ");
        Serial.println(texto);

        if (texto == "/ligar") {
          relayLigado = true;
          digitalWrite(RELAY_PIN, HIGH);
          enviarTelegram("Ar-Condicionado LIGADO.");

        } else if (texto == "/desligar") {
          relayLigado = false;
          digitalWrite(RELAY_PIN, LOW);
          enviarTelegram("Ar-Condicionado DESLIGADO.");

        } else if (texto == "/status") {
          float t, h;
          if (!lerSensor(t, h)) {
            enviarTelegram("Erro ao ler sensor BME680.");
          } else {
            String estado = relayLigado ? "LIGADO" : "DESLIGADO";
            enviarTelegram("Temp: " + String(t, 1) + "°C  " +
                           "Umidade: " + String(h, 1) + "%  " +
                           "Ar-Condicionado: " + estado);
          }

        } else {
          enviarTelegram("Comando invalido. Use: /ligar /desligar /status");
        }
      }
      pos = idxId + 1;
    }
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  sensorOk = bme.begin(0x76);
  if (!sensorOk) {
    Serial.println("[SENSOR] 0x76 falhou, tentando 0x77...");
    sensorOk = bme.begin(0x77);
  }

  if (!sensorOk) {
    Serial.println("[SENSOR] BME680 nao encontrado! Verifique a ligacao.");
  } else {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    Serial.println("[SENSOR] BME680 inicializado com sucesso.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nCONECTADO!");
  Serial.println(WiFi.localIP());

  String msgInicio = sensorOk
    ? "Estufa online com BME680! Comandos: /ligar /desligar /status"
    : "Estufa online MAS sensor BME680 com FALHA. Verifique a ligacao.";

  enviarTelegram(msgInicio);
  ultimoEnvio = millis();
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoUpdate >= INTERVALO_POLLING) {
    ultimoUpdate = agora;
    verificarComandos();
  }

  if (agora - ultimoEnvio >= INTERVALO_STATUS) {
    ultimoEnvio = agora;

    float temp, umi;
    String estado = relayLigado ? "LIGADO" : "DESLIGADO";

    if (lerSensor(temp, umi)) {
      enviarTelegram("Relatorio - Temp: " + String(temp, 1) + "°C  " +
                     "Umidade: " + String(umi, 1) + "%  " +
                     "Ar-Condicionado: " + estado);
    } else {
      enviarTelegram("Erro ao ler sensor BME680.");
    }
  }
}