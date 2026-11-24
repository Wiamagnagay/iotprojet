# TP2 – Infrastructure MQTT et Node-RED

**Objectif** : faire communiquer des ESP32 avec une Raspberry Pi grâce au protocole **MQTT**, puis afficher les données et commander les ESP32 depuis **Node-RED**.

Pour ce TP, nous avons repris les exemples du [dépôt du cours](https://github.com/AlCoISIS/TpMqttNodeRed) :
- un ESP32 avec un capteur de température / humidité **DHT11** ;
- un ESP32 avec une **LED** ;
- un ESP32 avec un **bouton**.

Cette infrastructure sert ensuite de base au [projet de surveillance cardiaque](Projet-Surveillance-cardiaque).

## 1. Adresses du réseau

| Appareil | Adresse IP |
|---|---|
| Raspberry Pi (Mosquitto + Node-RED) | `172.18.32.44` |
| ESP32 bouton | `172.18.32.134` |
| ESP32 capteur de chaleur (DHT11) | `172.18.32.125` |
| ESP32 LED | `172.18.32.114` |

> Ces adresses sont données par le réseau (DHCP) et peuvent changer d'une séance à l'autre. On les vérifie avec la commande `hostname -I` sur la Raspberry, et dans le moniteur série pour les ESP32 (ligne « IP address »).

## 2. Installation sur la Raspberry Pi

### Broker MQTT Mosquitto
Tutoriel : [Install Mosquitto MQTT Broker on Raspberry Pi](https://randomnerdtutorials.com/how-to-install-mosquitto-broker-on-raspberry-pi/)

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable mosquitto.service
```

Depuis la version 2, Mosquitto n'accepte que les connexions venant de la Raspberry elle-même. Pour autoriser les ESP32, on crée le fichier `/etc/mosquitto/conf.d/reseau.conf` :

```text
listener 1883
allow_anonymous true
```

puis on redémarre : `sudo systemctl restart mosquitto`.

Test : dans un terminal, `mosquitto_sub -h localhost -t "#" -v` affiche tous les messages reçus par le broker.

### Node-RED
Tutoriel : [Running Node-RED on Raspberry Pi](https://nodered.org/docs/getting-started/raspberrypi)

```bash
bash <(curl -sL https://github.com/node-red/linux-installers/releases/latest/download/install-update-nodered-deb)
sudo systemctl enable nodered.service
node-red-start
```

L'éditeur est accessible à l'adresse `http://172.18.32.44:1880`.

### Dashboard 2.0
Dans Node-RED : menu ☰ → **Gérer la palette** → onglet **Installer** → chercher et installer **`@flowfuse/node-red-dashboard`**. Le dashboard est ensuite accessible sur `http://172.18.32.44:1880/dashboard`.

### Accès sécurisé à Node-RED
L'éditeur Node-RED est protégé par un identifiant et un mot de passe. Ils ne sont volontairement pas publiés sur ce wiki : on peut les demander à l'équipe si besoin.

## 3. Codes ESP32 (exemples du cours)

| ESP32 | Code du dépôt du cours | Topic MQTT | Sens |
|---|---|---|---|
| Capteur DHT11 | `esp32dht11` | `esp32/DHT11/temperature`, `esp32/DHT11/humidity` | ESP32 → Node-RED |
| LED | `esp32led` | `esp32/output` (`on` / `off`) | Node-RED → ESP32 |
| Bouton | `esp32Button` | `esp32/button1` (`down` / `up`) | ESP32 → Node-RED |

Dans chaque code, il faut renseigner :
- le nom et le mot de passe du WiFi ;
- l'adresse IP du broker, ici celle de la Raspberry `172.18.32.44`.

Chaque ESP32 doit utiliser un identifiant de client MQTT différent (`ESP32Client_DHT`, `ESP32Client_Led`, `ESP32Client_Btn`).

## 4. Flow Node-RED et dashboard

Nous avons importé le flow du cours (`nodeRedFlows/flowsTpMqttNodeRed.json`). Il contient :
- deux graphiques (température et humidité) alimentés par les topics du DHT11 ;
- un interrupteur « LED status » qui publie sur `esp32/output` ;
- l'état du bouton (`down` / `up`), converti en `on` / `off` par une fonction et affiché sur l'interrupteur.

<img width="1088" height="531" alt="Capture TP2" src="https://github.com/user-attachments/assets/29ae2553-af8f-49e6-aa91-422aefc7e87f" />
<img width="958" height="483" alt="Capture TP2" src="https://github.com/user-attachments/assets/db31107b-02b1-411d-9802-780e0358c0f0" />

---
⬅️ [TP1 – Configuration de l'ESP32](TP1-Configuration-ESP32) · ➡️ [TP3 – I2C, FreeRTOS et capteur cardiaque](TP3-I2C-FreeRTOS-et-capteur)
