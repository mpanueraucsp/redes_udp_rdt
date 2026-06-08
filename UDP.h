#ifndef UDP_H
#define UDP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;
struct StandarPacketData {
    string type = "1";          // Tipo de paquete (ej. datos, ACK, control)
    int seqNum = 0;             // Número de secuencia del paquete
    string checkSum = "0";      // Checksum calculado para verificar integridad
    int nPakects = 0;           // Total de paquetes en los que se dividió el mensaje
    int actualPakect = 0;       // Índice del paquete actual (0-based)
    string fileData = "";       // Payload: fragmento de datos a transmitir
};


class UDPPacket {
private:
    string buf;
    int sizeBuf = 500;
    int actualSizeBuf; 
    int actualReadBuf; 
    
    char padding = '@';

    int modul = 94;
    int sizeChecksum = 4;
    int idxSetChecksum = 9;
    int checksumIdxBegin = 22;
    
    int header = 23;

public:
    UDPPacket() { 
        buf.assign(sizeBuf, padding);
        actualSizeBuf = 0; 
        actualReadBuf = 0;
    }

    void loadFromBuffer( char* data) {
        buf.assign(data, sizeBuf);
        actualReadBuf = 0; 
    }
    
    void addSizeAndMsg( string& msg, int bytesSize) {
        addSize(msg.size(), bytesSize); 
        addMsg(msg);
    }

    void addSize(int size, int bytesSize) {
        char formato[50]={0};
        snprintf(formato, sizeof(formato), "%%0%dd", bytesSize); 
        char sizeStr[50]={0};
        snprintf(sizeStr, sizeof(sizeStr), formato, size); 
        for (int i = 0; i < bytesSize; ++i) 
            buf[actualSizeBuf++] = sizeStr[i];
    }

    void addMsg( string& msg) {
        for(size_t i = 0; i < msg.size(); ++i)
            buf[actualSizeBuf++] = msg[i];
    }

    void addMsg(char c) {
        buf[actualSizeBuf++] = c;
    }

    int readSize(int bytesSize) {
        string sizeStr = buf.substr(actualReadBuf, bytesSize);
        actualReadBuf += bytesSize;         
        return stoi(sizeStr);
    }

    string readSizeAndMsg(int bytesSize, int& numberInBytesSize) {
        numberInBytesSize = readSize(bytesSize);
        return readMsg(numberInBytesSize);
    }

    string readMsg(int bytesSize) {
        string msg = buf.substr(actualReadBuf, bytesSize);
        actualReadBuf += bytesSize;
        return msg;
    }

    char calCheckSum(int idxBegin)  {
        long long sum = 0;
        for (int i = idxBegin; i < sizeBuf; i++) {
            sum += (unsigned char)buf[i] * (i - idxBegin + 1);
        }
        sum = (sum % modul) + 32;
        return (char)(sum);
    }

    bool verifyChecksum()  { 
        return calCheckSum(checksumIdxBegin) == buf[idxSetChecksum]; 
    }
    void setChecksum() { 
        buf[idxSetChecksum] = calCheckSum(checksumIdxBegin); 
    }
};


#endif