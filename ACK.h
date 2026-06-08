#ifndef ACK_H
#define ACK_H

#include "UDP.h"
#include <map>
#include <deque>
#include <string>

using namespace std;

class ACK {
private:
    UDP* udp;

    // KNOW IF ITS ACK OR NACK
    int lastAckSeq = -1;
    int lastNackSeq = -1;

    // CONFIG CORRUPCION
    bool enableCorrupt = true;

    // CONFIG PACKET LOSS
    bool enablePacketLoss = true;

    // TIMEOUT
    int timeout = 3;
    int maxRetry = 5;
    
    // DETECTAR DUPLICADOS
    map<string, deque<int>> dupePackets;


public:
    ACK(UDP* u) {
        udp = u;
    }

    // PARA CLIENTE
    bool sendPacket(const UDPPacket& p) {

    }

    // PARA SERVER 
    bool sendPacketTo(const UDPPacket& p, struct sockaddr_in dest) {

    }

    // PARA SERVER y SERVER
    bool receivePacket(char* buffer) {
        
    }
};

#endif