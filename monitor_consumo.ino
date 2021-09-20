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

long long tempo_espera = 5000;
unsigned long long inicio;
unsigned long long agora;
unsigned long long antes;
unsigned long long tempo_decorrido;

String AP = "Arya";
String PASS = "Got-061117";
String HOST = "192.168.15.114";
String PORT = "80";
String Data = "";
String dados = "";
double potencia_media;
double temperatura_media;

boolean DEBUG = true;
boolean found = false;
boolean enviou = true;
boolean hora_sincronizada = false;

int countTimeCommand;
int count_n_enviou;

EnergyMonitor monitor_corrente;
EnergyMonitor monitor_ruido;

SoftwareSerial esp8266(rx_pin, Tx_pin);

void setup() {
    inicio = millis();
    Serial.begin(9600);
    Serial.println();
    Serial.println("Iniciando");

    esp8266.begin(115200);
    sendCommand("AT", 5, "OK", false);
    esp8266.println("AT+UART_DEF=9600,8,1,0,0");
    delay(1000);
    esp8266.end();
    esp8266.begin(9600);

    connectToWifi();

    pinMode(pin_lm35, INPUT);
    monitor_corrente.current(pin_corrente, 6.707);
    monitor_ruido.current(pin_ruido, 6.0607);
}

void loop() {
    unsigned long long inicio = millis();
    if (DEBUG) {
        Serial.println("Loop inicio");
    }

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

    unsigned long long tempo_enviar = millis();
    agora += (tempo_enviar - inicio);

    if ((agora - antes) > tempo_espera) {
        tempo_decorrido = agora - antes;
    } else {
        tempo_decorrido = tempo_espera;
    }

    if (enviou) {
        potencia_media = potencia;
        temperatura_media = temperatura;
    } else {
        potencia_media += potencia;
        temperatura_media += temperatura;
        potencia = potencia_media / (count_n_enviou + 1);
        temperatura = temperatura_media / (count_n_enviou + 1);
    }

    double consumo = (potencia / 1000 / (3600 / (tempo_decorrido / 1000)));
    String dados =
            String(potencia) + "," +
            String(temperatura) + "," +
            String(consumo, 8) +
            ",MedidorAvulso," +
            toString(agora);

    String uri = "/api/v1/consumo?dados=" + dados;

    String getData =
            "POST " + uri + " HTTP/1.0\n" +
            "Host: " + HOST + "\n" +
            "Connection: close\n" +
            "\n";

    if (!(hora_sincronizada &&
          sendCommand("AT+CIPMUX=1", 5, "OK", false) &&
          sendCommand("AT+CIPSTART=4,\"TCP\",\"" + HOST + "\"," + PORT, 2, "OK", false) &&
          sendCommand("AT+CIPSEND=4," + String(getData.length() + 4), 4, ">", false) &&
          sendCommand(getData, 20, "OK", false))) {
        enviou = false;
        count_n_enviou++;
        checkWifiConnect();
    } else {
        enviou = true;
        count_n_enviou = 0;
        antes = agora;
    }

    unsigned long long fim = millis();
    long long espera = tempo_espera - (fim - inicio);
    if (espera > 0) {
        delay(espera);
    }

    if (DEBUG) {
        Serial.println("Loop fim");
    }

    agora += (millis() - tempo_enviar);
}

bool checkWifiConnect() {

    if (sendCommand("AT+CIPSTATUS=?", 5, "OK", true)) {
        Serial.println("Dados: " + dados);
        if (dados.lastIndexOf("WIFI DISCONNECT") > 0) {
            if (DEBUG) {
                Serial.println("Reconectando");
            }
            return connectToWifi();
        } else {
            if (!hora_sincronizada) {
                horaAtual();
            }
            if (DEBUG) {
                Serial.println("Conectado");
            }
        }
        return true;
    } else {
        esp8266.end();
        delay(1000);
        esp8266.begin(115200);
        sendCommand("AT", 5, "OK", false);
        esp8266.println("AT+UART_DEF=9600,8,1,0,0");
        delay(1000);
        esp8266.end();
        esp8266.begin(9600);
        return connectToWifi();
    }
}

void horaAtual() {
    String uri = "/api/v1/data-hora";
    String getData =
            "GET " + uri + " HTTP/1.0\n" +
            "Host: " + HOST + "\n" +
            "\n";

    if (sendCommand("AT+CIPMUX=1", 5, "OK", false) &&
        sendCommand("AT+CIPSTART=4,\"TCP\",\"" + HOST + "\"," + PORT, 2, "OK", false) &&
        sendCommand("AT+CIPSEND=4," + String(getData.length() + 4), 4, ">", false)) {
        if (sendCommand(getData, 20, "OK", true)) {
            unsigned long long y = 0;
            int fim = dados.lastIndexOf(",CLOSED");
            if (fim > 0) {
                String agora_st = "";
                for (int i = fim; i > 0; i--) {
                    if (!isDigit(dados.charAt(i))) {
                        if (dados.charAt(i) == '\n' and i < dados.length()) {
                            agora_st = dados.substring(i, (fim - 1));
                            i = 0;
                        }
                    }
                }

                dados = agora_st;
                if (DEBUG) {
                    Serial.println(dados);
                }

                for (int i = 0; i < dados.length(); i++) {
                    char c = dados.charAt(i);
                    y *= 10;
                    y += String(c).toInt();
                }

                if (DEBUG) {
                    Serial.println(toString(y));
                }
                agora = y;
                if (!hora_sincronizada) {
                    antes = agora - (millis() - inicio);
                }
                hora_sincronizada = true;
            }
            dados = "";
        }
    }
}

String toString(long long input) {
    String result = "";
    uint8_t base = 10;
    do {
        char c = input % base;
        input /= base;

        if (c < 10)
            c += '0';
        else
            c += 'A' - 10;
        result = c + result;
    } while (input);
    return result;
}

bool connectToWifi() {
    if (DEBUG) {
        Serial.println("Conectando a " + AP);
    }
    sendCommand("AT", 5, "OK", false);
    sendCommand("AT+CWMODE=1", 5, "OK", false);
    boolean isConnected = sendCommand("AT+CWJAP=\"" + AP + "\",\"" + PASS + "\"", 20, "OK", false);
    if (isConnected) {
        horaAtual();
    }
    return isConnected;
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
        if (esp8266.find(readReplay)) {
            if (isGetData) {
                dados = esp8266.readString();
                if (DEBUG) {
                    Serial.println(dados);
                }
                if (esp8266.find(readReplay)) {
                    if (DEBUG) {
                        Serial.println("Success : Request is taken from the server");
                    }
                }
                while (esp8266.available()) {
                    char character = esp8266.read();
                    Data.concat(character);
                    if (DEBUG) {
                        Serial.println(Data);
                    }
                    if (character == '\n') {
                        if (DEBUG) {
                            Serial.print("Received: ");
                            Serial.println(Data);
                        }
                        delay(50);
                        dados = Data;
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