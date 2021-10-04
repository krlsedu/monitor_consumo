#include <Arduino.h>
#include "EmonLib.h"
#include "SoftwareSerial.h"

#define pin_corrente_1 A3
#define pin_corrente_2 A6
#define pin_ruido_1 A2
#define pin_ruido_2 A5
#define pin_lm35 A4
#define rx_pin 3
#define Tx_pin 2

boolean DEBUG = false;
boolean BIFASE = false;

int REDE = 127;
double CALIBRAGEM = 42.55319;

const String AP = "Arya";
const String PASS = "Got-061117";
String MC_SERVER = "192.168.15.85";
const String PORT = "80";
const String DISPOSITIVO = "WS";

boolean enviou = true;
boolean hora_sincronizada = false;

int count_n_sincronizou = 0;

unsigned long long antes = 0;

String Data = "";
String dados = "";

double potencia_media;

double temperatura_media;
int countTimeCommand;

int count_n_enviou;
unsigned long long inicio;
unsigned long long agora;
unsigned long long tempo_decorrido;

EnergyMonitor monitor_corrente_1;
EnergyMonitor monitor_ruido_1;
EnergyMonitor monitor_corrente_2;
EnergyMonitor monitor_ruido_2;

SoftwareSerial esp8266(rx_pin, Tx_pin);

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

bool sendCommand(String &command, int maxTime, char readReplay[], boolean isGetData) {
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
        Serial.print(". at command => ");
        Serial.print(command);
        Serial.print(" ");
        Serial.println("Fail");
        countTimeCommand = 0;
    }

    return result;
}


void horaAtual() {
    String uri = "/api/v1/data-hora";
    String comand_3 = "GET ";
    comand_3.concat(uri);
    comand_3.concat(" HTTP/1.0\n");
    comand_3.concat("Host: ");
    comand_3.concat(MC_SERVER);
    comand_3.concat("\n\n");

    String comand_2 = "AT+CIPSTART=4,\"TCP\",\"";
    comand_2 += MC_SERVER;
    comand_2 += "\",";
    comand_2 += PORT;

    String command_1 = "AT+CIPMUX=1";
    if (sendCommand(command_1, 20, "OK", false) &&
        sendCommand(comand_2, 10, "OK", false) &&
        sendCommand("AT+CIPSEND=4," + String(comand_3.length() + 4), 4, ">", false)) {
        if (sendCommand(comand_3, 20, "OK", true)) {
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
                count_n_sincronizou = 0;
            } else {
                count_n_sincronizou++;
            }
            dados = "";
        } else {
            count_n_sincronizou++;
        }
    } else {
        count_n_sincronizou++;
    }
}

bool connectToWifi() {
    if (DEBUG) {
        Serial.println(String("Conectando a ").concat(AP));
    }

    String con_wifi = "AT+CWJAP=\"";
    con_wifi.concat(AP);
    con_wifi.concat("\",\"");
    con_wifi.concat(PASS);
    con_wifi.concat("\"");

    String command_at = "AT";
    sendCommand(command_at, 5, "OK", false);
    String comand_mode = "AT+CWMODE=1";
    sendCommand(comand_mode, 5, "OK", false);
    boolean isConnected = sendCommand(con_wifi, 20, "OK", false);
    if (isConnected) {
        horaAtual();
    }
    return isConnected;
}

void inicializa() {
    esp8266.begin(115200);
    String comando_at = "AT";
    String comando_vel = "AT+UART_DEF=9600,8,1,0,0";
    sendCommand(comando_at, 5, "OK", false);
    esp8266.println(comando_vel);
    delay(1000);
    esp8266.end();
    esp8266.begin(9600);
}

void setup() {
    inicio = millis();
    Serial.begin(9600);
    Serial.println();
    Serial.println("Iniciando");

    inicializa();

    connectToWifi();

    pinMode(pin_lm35, INPUT);
    monitor_corrente_1.current(pin_corrente_1, CALIBRAGEM);
    monitor_ruido_1.current(pin_ruido_1, CALIBRAGEM);
    if (BIFASE) {
        monitor_corrente_2.current(pin_corrente_2, CALIBRAGEM);
        monitor_ruido_2.current(pin_ruido_2, CALIBRAGEM);
    }
}

void reinicializa() {
    esp8266.end();
    delay(1000);
    esp8266.begin(115200);
    esp8266.begin(9600);
    String comando_at = "AT";
    String comando_rst = "AT+RST";
    String comando_vel = "AT+UART_DEF=9600,8,1,0,0";
    sendCommand(comando_at, 5, "OK", false);
    sendCommand(comando_rst, 5, "OK", false);
    sendCommand(comando_at, 5, "OK", false);
    esp8266.println(comando_vel);
    delay(1000);
    esp8266.end();
    esp8266.begin(9600);
}

bool checkWifiConnect() {
    String comando = "AT+CIPSTATUS=?";
    if (sendCommand(comando, 5, "OK", true)) {
        if (DEBUG) {
            Serial.println("Dados: " + dados);
        }
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
            if (count_n_sincronizou >= 15 || (count_n_enviou % 100 == 0)) {
                String command = "AT+CWQAP";
                if (sendCommand(command, 20, "OK", false)) {
                    reinicializa();
                    if (connectToWifi()) {
                        count_n_sincronizou = 0;
                    }
                }
            }
        }
        return true;
    } else {
        reinicializa();
        return connectToWifi();
    }
}

void enviaDados(const String payloadEnviar) {
    String uri = "/api/v1/consumo?dados=";
    uri.concat(payloadEnviar);

    String comando_4 = "POST ";
    comando_4.concat(uri);
    comando_4.concat(" HTTP/1.1\nHost: ");
    comando_4.concat(MC_SERVER);
    comando_4.concat("\nConnection: close\n\n");

    String comando_2 = "AT+CIPSTART=4,\"TCP\",\"";
    comando_2 += MC_SERVER;
    comando_2 += "\",";
    comando_2 += PORT;

    String comando_3 = "AT+CIPSEND=4,";
    comando_3 += String(comando_4.length() + 4);

    String command_1 = "AT+CIPMUX=1";
    if (!(hora_sincronizada &&
          sendCommand(command_1, 5, "OK", false) &&
          sendCommand(comando_2, 10, "OK", false) &&
          sendCommand(comando_3, 4, ">", false) &&
          sendCommand(comando_4, 20, "OK", false))) {
        enviou = false;

        count_n_enviou++;

        checkWifiConnect();
    } else {
        enviou = true;

        count_n_enviou = 0;
        antes = agora;
    }
}

float getTemperatura() {
    float temperatura = 0;
    for (int i = 0; i < 10; ++i) {
        float valor_analog_lm35 = float(analogRead(pin_lm35));
        float tensao = (valor_analog_lm35 * 5) / 1024;
        temperatura += tensao * 100;
    }
    temperatura = temperatura / 10;
    return temperatura;
}

double getPotencia(EnergyMonitor monitor_c, EnergyMonitor monitor_r) {
    double corrente = monitor_c.calcIrms(1480);

    double ruido = monitor_r.calcIrms(1480);

    corrente -= ruido;

    if (corrente < 0) {
        corrente = 0;
    }

    double potencia = corrente * REDE;

    if (potencia < 2) {
        potencia = 0;
    }

    return potencia;
}

void loop() {
    unsigned long long inicio = millis();

    if (DEBUG) {
        Serial.println("Loop inicio");
    }

    float temperatura = getTemperatura();

    double potencia = getPotencia(monitor_corrente_1, monitor_ruido_1);

    if (BIFASE) {
        potencia += getPotencia(monitor_corrente_2, monitor_ruido_2);
    }

    unsigned long long tempo_enviar = millis();

    agora += (tempo_enviar - inicio);

    tempo_decorrido = agora - antes;

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

    String payloadEnviar = String(potencia);
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(temperatura));
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(consumo, 8));
    payloadEnviar.concat(",");
    payloadEnviar.concat(DISPOSITIVO);
    payloadEnviar.concat(",");
    payloadEnviar.concat(toString(agora));
    payloadEnviar.concat(",");
    payloadEnviar.concat(toString(tempo_decorrido));

    Serial.println(payloadEnviar);

    enviaDados(payloadEnviar);

    if (DEBUG) {
        Serial.println("Loop fim");
    }
    agora += (millis() - tempo_enviar);
}
