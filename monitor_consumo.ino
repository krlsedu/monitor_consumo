#include <Arduino.h>
#include "EmonLib.h"
#include "SoftwareSerial.h"

#define pin_lm35 A1
#define pin_corrente A0
#define pin_ruido A2
#define pin_ruido A2
#define rx_pin 2
#define Tx_pin 3

int rede = 127;

long tempo_espera = 5000;

String AP = "Arya";
String PASS = "Got-061117";
String HOST = "192.168.15.114";
String PORT = "80";
String Data = "";

boolean DEBUG = false;
boolean found = false;

int countTimeCommand;

EnergyMonitor monitor_corrente;
EnergyMonitor monitor_ruido;

SoftwareSerial esp8266(rx_pin, Tx_pin);

void setup() {
    Serial.begin(9600);
    Serial.println();
    Serial.println("Iniciando");

    esp8266.begin(115200);
    sendCommand("AT", 5, "OK", false);
    esp8266.println("AT+UART_DEF=9600,8,1,0,0");
    delay(1000);
    esp8266.end();
    esp8266.begin(9600);
    while (!ConnectToWifi()) {
        if (DEBUG) {
            Serial.println("Conectando a " + AP);
        }
    }

    pinMode(pin_lm35, INPUT);
    monitor_corrente.current(pin_corrente, 6.707);
    monitor_ruido.current(pin_ruido, 6.0607);
}

void loop() {
    unsigned long inicio = millis();

    float temperatura = 0;
    for (int i = 0; i < 10; ++i) {
        float valor_analog_lm35 = float(analogRead(pin_lm35));
        float tensao = (valor_analog_lm35 * 5) / 1023;
        temperatura += tensao * 100;
    }
    temperatura = temperatura / 10;


    double corrente = monitor_corrente.calcIrms(1480);

    double ruido = monitor_ruido.calcIrms(1480);

    corrente -= ruido;

    if (corrente < 0) {
        corrente = 0;
    }

    double potencia = corrente * rede;

    if (potencia < 2) {
        potencia = 0;
    }

    double consumo = (potencia / 1000 / (3600 / (tempo_espera / 1000)));

    String dados = String(potencia) + "," + String(temperatura) + "," + String(consumo, 8) + ",MedidorAvulso";

    String uri = "http://" + HOST + ":" + PORT + "/api/v1/consumo?dados=" + dados;

    String getData =
            "POST " + uri + " HTTP/1.0\n" +
            "Host: " + HOST + "\n" +
            "Connection: close\n" +
            "\n";

    sendCommand("AT+CIPMUX=1", 5, "OK", false);
    sendCommand("AT+CIPSTART=4,\"TCP\",\"" + HOST + "\"," + PORT, 15, "OK", false);
    sendCommand("AT+CIPSEND=4," + String(getData.length() + 4), 4, ">", false);

    sendCommand(getData, 20, "OK", false);

    unsigned long fim = millis();
    long espera = tempo_espera - (fim - inicio);
    Serial.println(espera);
    if (espera > 0) {
        delay(espera);
    }
}

bool ConnectToWifi() {
    for (int a = 0; a < 15; a++) {
        sendCommand("AT", 5, "OK", false);
        sendCommand("AT+CWMODE=1", 5, "OK", false);
        boolean isConnected = sendCommand("AT+CWJAP=\"" + AP + "\",\"" + PASS + "\"", 20, "OK", false);
        if (isConnected) {
            return true;
        }
    }
}

bool sendCommand(const String &command, int maxTime, char readReplay[], boolean isGetData) {
    boolean result = false;

    if (DEBUG) {
        Serial.print(". at command => ");
        Serial.print(command);
        Serial.print(" ");
    }
    while (countTimeCommand < (maxTime * 1)) {
        esp8266.println(command);
        if (esp8266.find(readReplay))//ok
        {
            if (isGetData) {
                if (esp8266.find(readReplay)) {
                    if (DEBUG) {
                        Serial.println("Success : Request is taken from the server");
                    }
                }
                while (esp8266.available()) {
                    char character = esp8266.read();
                    Data.concat(character);
                    if (character == '\n') {
                        if (DEBUG) {
                            Serial.print("Received: ");
                            Serial.println(Data);
                        }
                        delay(50);
                        Data = "";
                    }
                }
            }
            result = true;
            break;
        }
        countTimeCommand++;
    }

    if (result == true) {
        if (DEBUG) {
            Serial.println("Success");
        }
        countTimeCommand = 0;
    }

    if (result == false) {
        if (DEBUG) {
            Serial.println("Fail");
        }
        countTimeCommand = 0;
    }

    found = false;
    return result;
}