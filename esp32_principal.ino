// =====================================================================
//  ESP32 "NOEUD PRINCIPAL" : CAPTEUR CARDIAQUE + SLAVE I2C + MQTT
//
//  - lit le capteur DFRobot Gravity Heart Rate & Oximeter (MAX30102)
//  - envoie les mesures en MQTT vers la Raspberry (Node-RED) :
//      topic "seb/patient/donnees" :
//      {"code":0,"message":"...","bpm":72,"spo2":98}
//  - repond au Noeud de diagnostic en I2C (adresse 0x55) :
//      1 octet = code d'etat (meme table que le Noeud de diagnostic)
//
//  FreeRTOS : 2 taches en parallele
//   - tacheCapteur (coeur 1) : lit le capteur, met a jour le code d'etat
//                              et depose la mesure dans une file
//   - tacheMQTT    (coeur 0) : garde le WiFi/MQTT connectes et publie
//   -> si le WiFi tombe, le capteur et la liaison I2C continuent.
//
//  Cablage :
//   - vers le Noeud de diagnostic : GPIO21 (SDA), GPIO22 (SCL), GND
//   - capteur : vert D/T -> GPIO25, bleu C/R -> GPIO26,
//               rouge -> 3V3, noir -> GND
//
//  Bibliotheques : DFRobot_BloodOxygen_S, DFRobot_RTU, PubSubClient
// =====================================================================

#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DFRobot_BloodOxygen_S.h"

// ---------- A MODIFIER ----------
const char *ssid        = "iot";
const char *password    = "iotisis;";
const char *mqtt_server = "172.18.32.44";   // IP de la Raspberry

#define MQTT_CLIENT_ID "ESP32_Principal_SEB"   // different sur chaque ESP32
#define TOPIC_DONNEES  "seb/patient/donnees"

// ---------- I2C vers le Noeud de diagnostic ----------
#define I2C_DEV_ADDR 0x55

// ---------- Capteur cardiaque ----------
#define SENSOR_ADDR 0x57
#define SENSOR_SDA  25
#define SENSOR_SCL  26

TwoWire I2C_2 = TwoWire(1);
DFRobot_BloodOxygen_S_I2C MAX30102(&I2C_2, SENSOR_ADDR);


// =====================================
// TABLE DES CODES ERREUR (identique dans le Noeud de diagnostic)
// =====================================

// ----- Erreurs techniques -----
#define ERR_NONE              0
#define ERR_WIFI_LOST         1   // WiFi perdu
#define ERR_SENSOR_DISCONNECT 2   // capteur ne repond pas
#define ERR_SENSOR_READ       3   // valeurs incoherentes
#define ERR_MQTT_LOST         8   // WiFi OK mais broker injoignable

// ----- Erreurs au niveau du capteur cardiaque -----
#define ERR_NO_FINGER         4   // pas de doigt / mesure en cours
#define ERR_BPM_LOW           5   // pouls < BPM_MIN
#define ERR_BPM_HIGH          6   // pouls > BPM_MAX
#define ERR_SPO2_LOW          7   // SpO2 < SPO2_MIN

// Seuils d'alerte (les memes que dans Node-RED)
#define BPM_MIN   50
#define BPM_MAX   100
#define SPO2_MIN  90

const char* getErrorMessage(uint8_t code) {

  switch (code) {
    case ERR_NONE:              return "Fonctionnement nominal : acquisition capteur en cours";
    case ERR_WIFI_LOST:         return "Impossible de se connecter au WiFi";
    case ERR_SENSOR_DISCONNECT: return "Probleme liaison capteur (capteur debranche ?)";
    case ERR_SENSOR_READ:       return "Erreur de lecture du capteur (valeurs incoherentes)";
    case ERR_MQTT_LOST:         return "WiFi OK mais broker MQTT (Raspberry) injoignable";
    case ERR_NO_FINGER:         return "Capteur OK, en attente du doigt / mesure en cours";
    case ERR_BPM_LOW:           return "Alerte : frequence cardiaque trop basse";
    case ERR_BPM_HIGH:          return "Alerte : frequence cardiaque trop haute";
    case ERR_SPO2_LOW:          return "Alerte : saturation en oxygene trop basse";
    default:                    return "Code erreur non repertorie";
  }
}


// =====================================
// DONNEES PARTAGEES
// =====================================

// Une mesure du capteur (bpm / spo2 a 0 = pas de valeur valide)
struct Mesure {
  uint8_t code;
  int bpm;
  int spo2;
};

QueueHandle_t fileMesures;               // file FreeRTOS de 1 case

volatile uint8_t errorCode = ERR_SENSOR_DISCONNECT;  // envoye au Diagnostic
volatile bool wifiOK = false;
volatile bool mqttOK = false;

bool capteurOK = false;

WiFiClient espClient;
PubSubClient client(espClient);


// =====================================
// ENVOI I2C : appelee quand le Diagnostic demande l'etat (1 octet)
// =====================================

void onRequest() {
  Wire.write(errorCode);
}


// =====================================
// CAPTEUR CARDIAQUE
// =====================================

bool capteurPresent() {
  I2C_2.beginTransmission(SENSOR_ADDR);
  return I2C_2.endTransmission() == 0;
}

void connecterCapteur() {

  if (MAX30102.begin()) {
    MAX30102.sensorStartCollect();
    capteurOK = true;
    Serial.println("[CAPTEUR] OK : posez le doigt dessus et ne bougez plus");
  }
  else {
    capteurOK = false;
    Serial.println("[CAPTEUR] Introuvable : verifier le cablage (25/26, 3V3, GND)");
  }
}


// =====================================
// TACHE 1 : LECTURE DU CAPTEUR
// =====================================

void tacheCapteur(void *param) {

  connecterCapteur();

  for (;;) {

    Mesure m = { ERR_NONE, 0, 0 };

    // 1) Capteur debranche ? -> nouvelle tentative
    if (!capteurOK || !capteurPresent()) {
      capteurOK = false;
      connecterCapteur();
    }

    // 2) Lecture et code du capteur
    if (!capteurOK) {
      m.code = ERR_SENSOR_DISCONNECT;
    }
    else {
      MAX30102.getHeartbeatSPO2();
      int hr = MAX30102._sHeartbeatSPO2.Heartbeat;   // -1 = invalide
      int ox = MAX30102._sHeartbeatSPO2.SPO2;

      if (hr <= 0 || ox <= 0) {
        m.code = ERR_NO_FINGER;          // pas de doigt ou mesure en cours
      }
      else if (hr > 250 || ox > 100) {
        m.code = ERR_SENSOR_READ;        // valeurs impossibles
      }
      else {
        m.bpm  = hr;
        m.spo2 = ox;

        if (ox < SPO2_MIN)       m.code = ERR_SPO2_LOW;
        else if (hr > BPM_MAX)   m.code = ERR_BPM_HIGH;
        else if (hr < BPM_MIN)   m.code = ERR_BPM_LOW;
        else                     m.code = ERR_NONE;
      }
    }

    // 3) Code envoye au Diagnostic (1 seul code, par priorite) :
    //    capteur absent > WiFi perdu > broker perdu > etat de la mesure
    if (m.code == ERR_SENSOR_DISCONNECT) errorCode = ERR_SENSOR_DISCONNECT;
    else if (!wifiOK)                    errorCode = ERR_WIFI_LOST;
    else if (!mqttOK)                    errorCode = ERR_MQTT_LOST;
    else                                 errorCode = m.code;

    Serial.printf("[CAPTEUR] %d bpm | SpO2 %d %% | code I2C = %d (%s)\n",
                  m.bpm, m.spo2, errorCode, getErrorMessage(errorCode));

    // 4) Mesure deposee pour la tache MQTT (ecrase l'ancienne)
    xQueueOverwrite(fileMesures, &m);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}


// =====================================
// TACHE 2 : WIFI + MQTT
// =====================================

void connecterMQTT() {

  while (!client.connected() && WiFi.status() == WL_CONNECTED) {

    Serial.print("[MQTT] Connexion au broker... ");

    if (client.connect(MQTT_CLIENT_ID)) {
      Serial.println("OK");
    }
    else {
      Serial.printf("echec (rc=%d), nouvel essai dans 5 s\n", client.state());
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void tacheMQTT(void *param) {

  Mesure m;
  char json[160];

  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);

  for (;;) {

    // 1) WiFi (l'ESP32 se reconnecte tout seul s'il le perd)
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiOK) Serial.println("[WiFi] Connexion perdue...");
      wifiOK = false;
      mqttOK = false;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (!wifiOK) {
      Serial.print("[WiFi] Connecte, IP : ");
      Serial.println(WiFi.localIP());
      wifiOK = true;
    }

    // 2) Broker MQTT
    if (!client.connected()) {
      mqttOK = false;
      connecterMQTT();
    }
    mqttOK = client.connected();
    client.loop();

    // 3) Nouvelle mesure ? -> publication en JSON
    if (xQueueReceive(fileMesures, &m, pdMS_TO_TICKS(100)) == pdTRUE && mqttOK) {

      snprintf(json, sizeof(json),
               "{\"code\":%d,\"message\":\"%s\",\"bpm\":%d,\"spo2\":%d}",
               m.code, getErrorMessage(m.code), m.bpm, m.spo2);

      if (client.publish(TOPIC_DONNEES, json)) {
        Serial.printf("[MQTT] %s -> %s\n", TOPIC_DONNEES, json);
      }
      else {
        Serial.println("[MQTT] Echec de publication");
      }
    }
  }
}


// =====================================
// INITIALISATION
// =====================================

void setup() {

  Serial.begin(115200);

  // Bus 0 : esclave du Noeud de diagnostic
  Wire.onRequest(onRequest);
  Wire.begin((uint8_t)I2C_DEV_ADDR);

  // Bus 1 : maitre du capteur
  I2C_2.begin(SENSOR_SDA, SENSOR_SCL, 100000);

  fileMesures = xQueueCreate(1, sizeof(Mesure));

  //                      fonction      nom             pile  param prio handle coeur
  xTaskCreatePinnedToCore(tacheCapteur, "tacheCapteur", 4096, NULL, 2,   NULL,  1);
  xTaskCreatePinnedToCore(tacheMQTT,    "tacheMQTT",    8192, NULL, 1,   NULL,  0);

  Serial.println("ESP32 Noeud Principal initialise (2 taches FreeRTOS)");
}


// =====================================
// BOUCLE PRINCIPALE : vide, tout se passe dans les taches
// =====================================

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
