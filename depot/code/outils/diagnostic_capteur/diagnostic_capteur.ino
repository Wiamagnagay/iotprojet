// =====================================================================
//  DIAGNOSTIC : ou est branche le capteur cardiaque (adresse I2C 0x57) ?
//  A televerser sur l'ESP32 ou le capteur est branche (le Slave),
//  puis ouvrir le moniteur serie a 115200 bauds.
// =====================================================================

#include <Wire.h>

#define SENSOR_ADDR 0x57

// Broches testees (on evite 21/22 = bus vers le Master, et les broches sensibles)
const int broches[] = { 25, 26, 27, 4, 13, 14, 18, 19, 23, 32, 33 };
const int NB = sizeof(broches) / sizeof(broches[0]);

// Liste tous les peripheriques I2C trouves sur une paire de broches
int scanComplet(int sda, int scl) {
  int trouves = 0;
  Wire1.begin(sda, scl, 100000);
  Serial.printf("  SDA=GPIO%d  SCL=GPIO%d : ", sda, scl);
  for (uint8_t a = 1; a < 127; a++) {
    Wire1.beginTransmission(a);
    if (Wire1.endTransmission() == 0) {
      Serial.printf("0x%02X ", a);
      trouves++;
    }
  }
  Serial.println(trouves ? "" : "rien");
  Wire1.end();
  return trouves;
}

// Le capteur repond-il sur cette paire de broches ?
bool capteurSur(int sda, int scl) {
  Wire1.begin(sda, scl, 100000);
  Wire1.beginTransmission(SENSOR_ADDR);
  bool ok = (Wire1.endTransmission() == 0);
  Wire1.end();
  return ok;
}

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("===== DIAGNOSTIC CAPTEUR CARDIAQUE =====");

  // 1) Scan complet sur les broches prevues, dans les deux sens
  Serial.println("1) Scan sur GPIO25 / GPIO26 :");
  scanComplet(25, 26);
  scanComplet(26, 25);

  // 2) Le capteur est-il branche sur le bus du Master (21/22) par erreur ?
  Serial.println("2) Scan sur GPIO21 / GPIO22 (bus du Master) :");
  scanComplet(21, 22);
  if (capteurSur(21, 22)) {
    Serial.println();
    Serial.println("===== RESULTAT =====");
    Serial.println("Le capteur est branche sur SDA/SCL (21/22), le bus du Master.");
    Serial.println("-> Deplace D/T (vert) sur D2 = GPIO25");
    Serial.println("   et C/R (bleu) sur D3 = GPIO26 de cet ESP32.");
    return;
  }

  // 3) Recherche de 0x57 sur toutes les combinaisons de broches
  Serial.println("3) Recherche de 0x57 sur les autres broches...");
  int bonSDA = -1, bonSCL = -1;

  for (int i = 0; i < NB && bonSDA < 0; i++) {
    for (int j = 0; j < NB && bonSDA < 0; j++) {
      if (i != j && capteurSur(broches[i], broches[j])) {
        bonSDA = broches[i];
        bonSCL = broches[j];
      }
    }
  }

  // 4) Conclusion
  Serial.println();
  Serial.println("===== RESULTAT =====");

  if (bonSDA == 25 && bonSCL == 26) {
    Serial.println("Capteur trouve sur SDA=25 / SCL=26 : le cablage est BON.");
    Serial.println("-> Renvoie ce message, le probleme est ailleurs.");
  }
  else if (bonSDA >= 0) {
    Serial.printf("Capteur trouve sur SDA=GPIO%d / SCL=GPIO%d\n", bonSDA, bonSCL);
    Serial.println("-> Dans esp32_i2cSlave.ino, remplace :");
    Serial.printf("   #define SENSOR_SDA  %d\n", bonSDA);
    Serial.printf("   #define SENSOR_SCL  %d\n", bonSCL);
  }
  else {
    Serial.println("Capteur introuvable sur toutes les broches.");
    Serial.println("-> Probleme d'alimentation ou de mode :");
    Serial.println("   VCC(+) sur 3V3, GND(-) sur GND, fils bien enfonces,");
    Serial.println("   interrupteur I2C/UART sur I2C (s'il y en a un).");
  }
}

void loop() {
}
