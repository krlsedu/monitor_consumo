#include <Arduino.h>
#include "EmonLib.h"
#include "SoftwareSerial.h"
#include <EEPROM.h>

#define pin_corrente_1 A3
#define pin_corrente_2 A6
#define pin_lm35 A4
#define rx_pin 3
#define Tx_pin 2
#define rele_pin 4

boolean DEBUG = false;
boolean BIFASE = true;

int REDE = 127;
double CALIBRAGEM = 20;

const String AP = "Arya";
const String PASS = "Got-061117";
const String MC_SERVER = "192.168.15.85";
const String PORT = "80";
const String DISPOSITIVO = "GERAL";

boolean enviou = true;

String Data = "";
String dados = "";

double potencia_media;
double temperatura_media;

int countTimeCommand;

long count_n_enviou;
long agora;
long antes = 0;
long tempo_decorrido = 0;

EnergyMonitor monitor_corrente_1;
EnergyMonitor monitor_corrente_2;

SoftwareSerial esp8266(rx_pin, Tx_pin);

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

    return sendCommand(con_wifi, 20, "OK", false);
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

long leValorBase2x255(int endereco_base) {
    long base_255 = EEPROM.read(endereco_base) + EEPROM.read(endereco_base + 1);
    long resto = EEPROM.read(endereco_base + 2);
    long val = (base_255 * 255) + resto;
    return val;
}

void leEeprom() {
    int tem_dados = EEPROM.read(0);
    delay(1000);
    if (tem_dados == 1) {
        Serial.println("Lendo eeprom");
        enviou = false;

        int endereco_base = 1;
        potencia_media = leValorBase2x255(endereco_base);

        endereco_base = 4;
        long tempo_gravado = leValorBase2x255(endereco_base);
        antes -= (tempo_gravado * 1000);

        endereco_base = 7;
        count_n_enviou = leValorBase2x255(endereco_base);

        endereco_base = 10;
        temperatura_media = leValorBase2x255(endereco_base);

        potencia_media *= count_n_enviou;
        temperatura_media *= count_n_enviou;

        Serial.println("Lendo eeprom fim");
    }
}

void setup() {
    agora = (long) millis();
    pinMode(rele_pin, OUTPUT);

    Serial.begin(9600);
    Serial.println();
    Serial.println("Iniciando");
    leEeprom();

    inicializa();

    connectToWifi();

    pinMode(pin_lm35, INPUT);
    monitor_corrente_1.current(pin_corrente_1, CALIBRAGEM);
    if (BIFASE) {
        monitor_corrente_2.current(pin_corrente_2, CALIBRAGEM);
    }
}


void desliga() {
    digitalWrite(rele_pin, HIGH);
}


void gravaValorBase2x255(long val, int endereco_base) {
    int base_255 = (int) (val / 255);
    if (base_255 >= 1) {
        if (base_255 > 255) {
            EEPROM.update(endereco_base, 255);
            EEPROM.update(endereco_base + 1, base_255 - 255);
        } else {
            EEPROM.update(endereco_base, base_255);
            EEPROM.update(endereco_base + 1, 0);
        }
        int resto = (int) (val - (base_255 * 255));
        EEPROM.update(endereco_base + 2, resto);
    } else {
        EEPROM.update(endereco_base, 0);
        EEPROM.update(endereco_base + 1, 0);
        int resto = (int) val;
        EEPROM.update(endereco_base + 2, resto);
    }
}

void gravarEeprom() {
    if (enviou) {
        EEPROM.update(0, 0);
    } else {
        EEPROM.update(0, 1);
        int endereco_base = 1;
        double potencia = potencia_media / (count_n_enviou);
        gravaValorBase2x255((long) potencia, endereco_base);

        endereco_base = 4;
        tempo_decorrido += 2000;
        gravaValorBase2x255(tempo_decorrido / 1000, endereco_base);

        endereco_base = 7;
        gravaValorBase2x255(count_n_enviou, endereco_base);

        endereco_base = 10;
        double temp = temperatura_media / (count_n_enviou);
        gravaValorBase2x255((long) temp, endereco_base);
    }
}


void checkRestart() {
    gravarEeprom();
    if (count_n_enviou > 0 && (count_n_enviou % 5) == 0) {
        delay(2000);
        desliga();
    }

    if ((long) millis() >= 2100000000) {
        delay(2000);
        desliga();
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
    if (!(sendCommand(command_1, 5, "OK", false) &&
          sendCommand(comando_2, 10, "OK", false) &&
          sendCommand(comando_3, 4, ">", false) &&
          sendCommand(comando_4, 20, "OK", false))) {
        enviou = false;
        count_n_enviou++;
    } else {
        enviou = true;
        count_n_enviou = 0;
        antes = agora;
    }

    checkRestart();
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

double getPotencia(EnergyMonitor monitor_c, double ajuste) {
    double corrente = monitor_c.calcIrms(1480);

    double potencia = corrente * REDE;

    potencia -= ajuste;

    return potencia;
}

void loop() {

    if (DEBUG) {
        Serial.println("Loop inicio");
    }

    float temperatura = getTemperatura();

    double potencia = getPotencia(monitor_corrente_1, 15);

    if (BIFASE) {
        potencia += (getPotencia(monitor_corrente_2, 35) * 2);
    }

    if (potencia < 0) {
        potencia = 0;
    }

    agora = (long) millis();

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

    double consumo = (potencia / 1000 / (3600 / ((double) tempo_decorrido / 1000)));

    String payloadEnviar = String(potencia);
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(temperatura));
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(consumo, 8));
    payloadEnviar.concat(",");
    payloadEnviar.concat(DISPOSITIVO);
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(agora));
    payloadEnviar.concat(",");
    payloadEnviar.concat(String(tempo_decorrido));

    Serial.println(payloadEnviar);

    enviaDados(payloadEnviar);

    if (DEBUG) {
        Serial.println("Loop fim");
    }
}
