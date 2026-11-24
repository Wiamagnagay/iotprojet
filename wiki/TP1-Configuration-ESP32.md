# TP1 – Configuration de l'ESP32 et adresse MAC

## 1. Ajouter le lien de l'ESP32 dans l'Arduino IDE

1. Ouvrir **Fichier > Préférences**.
2. Dans **URL de gestionnaire de cartes supplémentaires**, coller :

   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

3. Cliquer sur **OK**.

## 2. Installer la bibliothèque ESP32

1. Ouvrir **Outils > Carte > Gestionnaire de cartes**.
2. Chercher **esp32**.
3. Installer **esp32 by Espressif Systems**.

Tutoriel de référence : [Installing ESP32 Board in Arduino IDE 2 – Random Nerd Tutorials](https://randomnerdtutorials.com/installing-esp32-arduino-ide-2-0/)

## 3. Choisir la carte et le port

1. **Outils > Carte > esp32** : sélectionner le modèle de la carte (par exemple *ESP32 Dev Module*, ou *FireBeetle-ESP32* pour nos cartes DFRobot).
2. Brancher l'ESP32 en USB.
3. **Outils > Port** : sélectionner le port de la carte.

## 4. Trouver l'adresse MAC

Nous avons suivi le tutoriel [Get/Change ESP32 MAC Address – Random Nerd Tutorials](https://randomnerdtutorials.com/get-change-esp32-esp8266-mac-address-arduino/).

1. Créer un nouveau croquis avec ce code :

   ```cpp
   #include <WiFi.h>

   void setup() {
     Serial.begin(115200);
     WiFi.mode(WIFI_STA);
     Serial.println(WiFi.macAddress());
   }

   void loop() {}
   ```

2. Cliquer sur **Téléverser**.
3. Ouvrir **Outils > Moniteur série** et choisir **115200 bauds**.
4. Appuyer sur le bouton **EN** (ou **RST**) de la carte.

## 5. Résultat

L'adresse MAC de nos ESP32 s'affiche dans le moniteur série :

| ESP32 | Adresse MAC |
|---|---|
| Carte A | `c8:f0:9e:e1:50:e8` |
| Carte B | `ec:62:60:65:b9:e8` |

---
⬅️ [Accueil](Home) · ➡️ [TP2 – MQTT et Node-RED](TP2-MQTT-et-Node-RED)
