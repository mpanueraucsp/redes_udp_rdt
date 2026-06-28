#ifndef ACK_H
#define ACK_H

#include "UDP.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <iostream>
#include <atomic>
#include <map>
#include <deque>
#include <algorithm>

using namespace std;

struct ACKPacketData {
    string type = "";
    int seqNum = 0;
};

class ACK {
private:
    UDP* udp;
    mutex seqMtx;

    // <ip:port, seqNum>
    // se usa un mapa puesto que se maneja los clientes por threads en el server, entonces cada cliente tiene su ack, nack que compara el seqNum
    map<string, int> lastAckSeqMap;
    map<string, int> lastNackSeqMap;

    // CONFIG CORRUPT (variables seteadas de ejemplo)
    bool enableCorrupt = false; // activar anomalia en paquetes
    int corruptInterval = 15; // aplicar anomalia(s) tras 15 pkt's enviados
    int counterCorruptInterval = 0; // contar para llegar a 15
    int corruptSequence = 2; // aplicar 2 anomalias de forma seguida
    int counterCorruptSequence = 0; // contar hasta las 2 anomalias
    int maxCorrupt = 10; // limite de aplicar la anomalia
    int totalCorruptApplied = 0; // contar hasta las 10 anomalias aplicadas
    int corruptIdxPkt = 24; // idx de aplicar cambio del byte

    // CONFIG PALCKETLOSS
    bool enablePacketLoss = false;
    int lossSequence = 3;
    int counterLossSequence = 0;
    int lossInterval = 20;
    int counterLossInterval = 0;
    int maxLoss = 10;
    int totalLossApplied = 0;

    // CONFIG ACK LOSS
    bool enableAckLoss = false;
    int ackLossSequence = 2;
    int counterAckLossSequence = 0;
    int ackLossInterval = 10;
    int counterAckLossInterval = 0;
    int maxAckLoss = 10;
    int totalAckLossApplied = 0;

    int timeout = 5000; // ms
    int maxRetry = 10; // max intentos de reenviar pkt

    bool printLogs = false;
    bool printRcv = true;

    // <ip:port, deque<seqNum>> buffer UDP
    // para guardar el seqNum de los pkt's recibidos y poder validar su hay pkt's duplicados
    map<string, deque<int>> dupePackets;

    // para contruir un ACK o NACK
    UDPPacket packACKToUDP(string type, int seqNum);

    // para leer un ACK o NACK
    ACKPacketData unpackUDPACK(UDPPacket& p);

    string getSenderId(struct sockaddr_in addr);

    // PLANTILLA PARA CONTROLAR LAS ANOMALIAS DE PAQUETES
    // sirve para saber si es momento de aplicar una anomalia retornando true, si retorna falso no se aplica la animalia
    bool checkAnomalia(
        bool enable, int maxLimit, int& totalApplied,
        int& counterSequence, int& counterInterval,
        int interval, int sequence
    );

    // PACKET LOSS
    bool checkPacketLoss();

    // CORRUPT
    bool checkCorrupt();

    // ACK LOSS TIMEOUT
    bool checkAckLoss();

    // metodo usado en sendPacketTo y evalua el TIMEOUT de la espera para recibir el ACK o NACK
    bool timeoutFun(int seqNum, string clientId);

    // imprimer el pkt
    void printHeader(UDPPacket p);

    // identificar PAQUETES DUPLICADOS y guardar el pkt en dupePackets caso no sea duplicado
    bool isDuplicated(const string& clientId, int seqNum);

public:
    ACK(UDP* u);

    bool sendPacket(const UDPPacket& p);

    // metodo usado para enviar paquetes y verificar el ACK o NACK recibido de receivePacket
    bool sendPacketTo(const UDPPacket& p, struct sockaddr_in dest, bool useDest = true);

    // metodo usado para recibir paquetes y verificar si es ACK, NACK o un paquete normal
    bool receivePacket(char* buffer);

    // setear las anomalias
    void setConfigTests(bool ec, int ci, int cs, int mc,
                        bool epl, int li, int ls, int ml,
                        bool eal, int ali, int als, int mal);
};

#endif
