// =====================================================================
//  ESP32 "NOEUD DE DIAGNOSTIC" (Master I2C)
//
//  Comme une valise de diagnostic : interroge le Noeud Principal en I2C
//  toutes les 2 s et affiche son etat de fonctionnement dans le
//  moniteur serie (115200 bauds). Pas de WiFi, pas de MQTT.
//
//  Cablage vers le Noeud Principal : GPIO21 (SDA), GPIO22 (SCL), GND
// =====================================================================

#include "Wire.h"

#define I2C_DEV_ADDR 0x55

// =====================================
// TABLE DES CODES ERREUR (identique dans le Noeud Principal)
// =====================================

// ----- Erreurs techniques -----
#define ERR_NONE              0
#define ERR_WIFI_LOST         1
#define ERR_SENSOR_DISCONNECT 2
#define ERR_SENSOR_READ       3
#define ERR_MQTT_LOST         8

// ----- Erreurs au niveau du capteur cardiaque -----
#define ERR_NO_FINGER         4
#define ERR_BPM_LOW           5
#define ERR_BPM_HIGH          6
#define ERR_SPO2_LOW          7


// =====================================
// CORRESPONDANCE CODE / DESCRIPTION
// =====================================

const char* getErrorMessage(uint8_t code) {

  switch (code) {

    // ----- Erreurs techniques -----

    case ERR_NONE:
      return "Fonctionnement nominal : acquisition capteur en cours";

    case ERR_WIFI_LOST:
      return "Impossible de se connecter au WiFi";

    case ERR_SENSOR_DISCONNECT:
      return "Probleme liaison capteur (capteur debranche ?)";

    case ERR_SENSOR_READ:
      return "Erreur de lecture du capteur (valeurs incoherentes)";

    case ERR_MQTT_LOST:
      return "WiFi OK mais broker MQTT (Raspberry) injoignable";

    // ----- Erreurs au niveau du capteur cardiaque -----

    case ERR_NO_FINGER:
      return "Capteur OK, en attente du doigt / mesure en cours";

    case ERR_BPM_LOW:
      return "Alerte : frequence cardiaque trop basse";

    case ERR_BPM_HIGH:
      return "Alerte : frequence cardiaque trop haute";

    case ERR_SPO2_LOW:
      return "Alerte : saturation en oxygene trop basse";

    default:
      return "Code erreur non repertorie";
  }
}


// =====================================
// INITIALISATION
// =====================================

void setup() {

  Serial.begin(115200);

  Wire.begin();

  Serial.println("ESP32 Diagnostic initialise");
}


// =====================================
// BOUCLE PRINCIPALE
// =====================================

void loop() {

  delay(2000);

  // Demande d'un seul octet au Noeud Principal
  uint8_t bytesReceived = Wire.requestFrom(
    (uint8_t)I2C_DEV_ADDR,
    (uint8_t)1
  );

  if (bytesReceived == 1 && Wire.available()) {

    // Lecture du code erreur
    uint8_t receivedCode = Wire.read();

    // Affichage du résultat
    Serial.println("-----------------------------");

    Serial.print("Code erreur : ");
    Serial.println(receivedCode);

    Serial.print("Description : ");
    Serial.println(getErrorMessage(receivedCode));

    Serial.println("-----------------------------");

  }
  else {

    Serial.println("Erreur : aucune reponse du Noeud Principal (liaison I2C ou alimentation)");
  }
}
