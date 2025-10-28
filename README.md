# indicateur_HP-HC à base d'ESP32 C3 SuperMini

## Prérequis:
- Une connexion internet avec Wifi
- Un abonnement EDF HP/HC
- Un ESP8266
- Une led bicouleurs rouge/vert

## But:
Indicateur visuel du cout de consommation electrique en fonction de l'heure de la journée afin de tenter de la limiter en sursoyant l'utilisation des machines consommatrice d'électricité

## Fonctionnement:
- Led rouge = Heures Pleines
- Led verte = Heures Creuses
- Led Orange clignotante = Le tarif heures va changer
- Vitesse de clignotement en fonction de l'échéance du changement

## Evolution: 
- Interface web type AP en cas de perte du wifi pour faire un nouvel appairage au routeur
- Interface de changement des horaires des heures creuses
- Interface de changement config serveur NTP
- Interface Config MQTT
