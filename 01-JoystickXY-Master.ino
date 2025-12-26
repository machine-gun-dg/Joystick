//START SERVER********************************************
#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"
// !! SOSTITUISCI CON IL MAC ADDRESS REALE DEL TUO SLAVE !!
// Esempio: {0x1C, 0xDB, 0xD4, 0x5A, 0xC3, 0x68}
uint8_t slaveAddress[] = {0x1C, 0xDB, 0xD4, 0x5A, 0xC3, 0x68}; 
// -- Inclusione delle definizioni (Sezione 1) --
enum Direction { NORD, EST, SUD, OVEST, INVALID_DIRECTION };
typedef struct struct_message { Direction direction; } struct_message;
const char* directionNames[] = {"NORD", "EST", "SUD", "OVEST"};
// ----------------------------------------------

struct_message myData;
esp_now_peer_info_t peerInfo;
int directionIndex = 0;
 

//END SERVER}*********************************************

// =======================================================
// CONFIGURAZIONE PIN E CALIBRAZIONE (DA MODIFICARE)
// =======================================================
const int joyX = 1; // VRx - Analog input per l'asse X (Sterzo / Direzione)
const int joyY = 2; // VRy - Analog input per l'asse Y (Accelerazione / Marcia)
const int buttonPin = 16; // SW - Digital input per il pulsante

// Costanti ADC dell'ESP32
const int ADC_MIN = 0;
const int ADC_MAX = 4095;

// ** VALORI CALIBRATI DEL TUO JOYSTICK **
const int CENTER_Y_REAL = 1750; // Il centro Y reale (Marcia)
const int CENTER_X_REAL = 1750; // Il centro X reale (Sterzo)

// Parametri di Mappatura
const int DEADZONE = 100; // Zona morta: es. 1750 +/- 100
const int STEP_PERCENT = 10; // Arrotonda al multiplo di 10%

// =======================================================
// FUNZIONE DI INIZIALIZZAZIONE (ESEGUITA 1 VOLTA)
// =======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("--- Mappatura 10% (INVERTITA) ---");
  Serial.println("Centro Y calibrato: " + String(CENTER_Y_REAL));
  
  pinMode(buttonPin, INPUT_PULLUP);
//START SERVER********************************************
WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
  
  // Registra il peer (lo Slave)
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("ESP-NOW Master Ready to send directions...");
//END SERVER}*********************************************

}


//START SERVER********************************************
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  
  // *** CORREZIONE DEFINITIVA: USA src_addr come suggerito dal compilatore ***
  // Questo ottiene il MAC address del dispositivo che ha inviato il pacchetto (il Master)
  const uint8_t * mac_addr = tx_info->src_addr; 
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Stampa il MAC address sorgente (Master) per conferma
    Serial.print("Packet sent by MAC: ");
    for(int i = 0; i < 6; i++) {
        Serial.printf("%02X%s", mac_addr[i], (i<5)?":":"");
    }
    Serial.println(" -> SUCCESS");
  } else {
    Serial.println("Packet failed to send.");
  }
}
//END SERVER********************************************


// =======================================================
// FUNZIONE DI MAPPATURA (LOGICA PRINCIPALE)
// =======================================================
/**
 * Mappa il valore analogico (0-4095) a una percentuale (-100 a +100)
 * usando un centro calibrato, arrotonda e INVERTE la direzione.
 */
int map_to_percentage(int value, int center) {
  
  // 1. Controllo Zona Morta (Deadzone)
  if (value >= (center - DEADZONE) && value <= (center + DEADZONE)) {
    return 0; // 0% (STOP/CENTRO)
  }

  // 2. Mappatura Lineare Calibrata (LOGICA INVERTITA)
  int raw_percentage;
  
  if (value > center) {
    // NUOVO: Mappa i valori sopra il centro (più alto) a 0 a -100 (Inversione)
    raw_percentage = map(value, center, ADC_MAX, 0, -100);
  } else {
    // NUOVO: Mappa i valori sotto il centro (più basso) a +100 a 0 (Inversione)
    raw_percentage = map(value, ADC_MIN, center, 100, 0); 
  }
  
  // 3. Arrotondamento al 10% più vicino
  int rounded_percentage = (int)round((float)raw_percentage / STEP_PERCENT) * STEP_PERCENT;

  // Garantisce che il valore massimo non superi il 100%
  if (rounded_percentage > 100) return 100;
  if (rounded_percentage < -100) return -100;

  return rounded_percentage;
}


// =======================================================
// FUNZIONE PRINCIPALE (ESEGUITA CONTINUAMENTE)
// =======================================================
void loop() {
  int xValue = analogRead(joyX);
  int yValue = analogRead(joyY);
  int buttonState = digitalRead(buttonPin);

  // Mappa usando i CENTRI CALIBRATI
  int yPercent = map_to_percentage(yValue, CENTER_Y_REAL);
  int xPercent = map_to_percentage(xValue, CENTER_X_REAL);

  // Stampa i risultati (Cleaned-up version)
  Serial.print("Y (Marcia): ");
  Serial.print(yPercent);
  Serial.print("% ("); 
  Serial.print(yValue); 
  Serial.print(") | X (Sterzo): ");
  Serial.print(xPercent);
  Serial.print("% ("); 
  Serial.print(xValue); 
  Serial.print(") | Bottone: ");
  
  if (buttonState == LOW) {
    Serial.println("**SCHIACCIATO**");
  } else {
    Serial.println("Rilasciato");
  }

//START SERVER********************************************
// Calcola la direzione da inviare (cicla tra 0, 1, 2, 3)
  Direction currentDirection = (Direction)(directionIndex % 4);
  
  myData.direction = currentDirection;
  
  Serial.print("\n--- SENDING --- Direction: ");
  Serial.println(directionNames[directionIndex % 4]);
  
  // Invia il messaggio
  esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));
  
  // Passa alla direzione successiva
  directionIndex++;
  
  delay(3000); // Invia una direzione ogni 3 secondi
//END SERVER********************************************


  //delay(100);
}
