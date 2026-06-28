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

void readIn(const char* msg, string& buffer);

// para definir los tipos de datos del protocolo
// el protocolo se arma en packDataToUDP
// el procolo se desarma en unpackUDPData
struct StandarPacketData {
    string type = "1";
    int seqNum = 0;
    string checkSum = "0";
    int nPakects = 0;
    int actualPakect = 0;
    int layer = 0;
    string fileData = "";
};

// sirve para porder armar y leer el protocolo
class UDPPacket {
private:
    string buf;
    int sizeBuf = 500;
    int actualSizeBuf;  // para saber desde que idx se debe sobreescribir
    int actualReadBuf;  // para saber desde que idx se debe leer

    char padding = '@';

    int modul = 94; // modulo para el checksum (94 caracteres normales ASCII)
    int idxSetChecksum = 9;  // idx del buff donde se debe setear el checksum
    int checksumIdxBegin = 24; // desde q idx se debe iniciar el calculo del checksum hasta el final del buffer

    bool printPaquetSend = true; // para imprimer paquetes que se envian
    static const bool printHederFormate = true; // para imprimer el header con formato y sea facil leer. Es static const porque se pasa como parametro
    int dataFileBytesPrint = 2; // print solo 2 bytes de los datos

public:
    UDPPacket();

    // carga el char del buffer recibido en receivePacket de UDP
    void loadFromBuffer(char* data);

    // setear padding desde el idx de tamano aplicado en el buffer
    void setPadding(char p);

    // ARMAR/AGREGAR AL BUFFER
    // agrega un tamano del msg y despues el msg
    void addSizeAndMsg(string& msg, int bytesSize);

    // agrega el tamano en un formato
    void addSize(int size, int bytesSize);

    // agrega un msg
    void addMsg(string& msg);

    // sobreacarga, agrega un msg char
    void addMsg(char c);

    // LEER EL BUFFER
    // leer un tamano y segun ese tamano lee y retorna el msg, por referencia retorna el tamano
    string readSizeAndMsg(int bytesSize, int& numberInBytesSize);

    // lee un tamano y retorna el numero contenido
    int readSize(int bytesSize);

    // lee un msg segun el numero de bytes y retorna el msg
    string readMsg(int bytesSize);

    // calcula el checksum desde un idx en el buff hasta el final del buff segun el sizeBuffer
    char calCheckSum(int idxBegin);

    // verifica el checksum sea correcto una vez cargo el buffer recibido
    bool verifyChecksum();

    // setea el checksum en su posicion definida idxSetChecksum
    void setChecksum();

    // imprime el buffer sin formato del header
    void printBuf() const;

    // imprime el header del buffer con un formato para poder leerlo comodamente
    void printHeader(const string tag = "\n", bool printFormate = printHederFormate) const;

    // retorna el buffer
    const string& getBuffer() const;

    // para setear el byte idx corrupto
    void setCorruptBuffer(int idx, char val);
    int getSize();
};

// clase para poder gestionar los fragmentos de un mensaje
class UDPMsg {
public:
    map<int, UDPPacket> UDPmsg;
    UDPMsg();
    void setFragment(int orden, UDPPacket fragment);
    int getSize();
};

// Para armar un paqute segun un protocolo
UDPPacket packDataToUDP(StandarPacketData& d);

// Para desarmar un paqute segun un protocolo
StandarPacketData unpackUDPData(UDPPacket& p);

// clase UDP para realizar la conexion del server y cliente, asi tambien permitirles enviar y recibir datos
class UDP {
private:
    int fd;
    struct sockaddr_in current_addr;
    socklen_t addr_len;
    int sizeBuf = 500;

public:
    UDP();
    ~UDP();

    // para que se conecte el server
    bool bindPort(int port);

    // para que se conecte el cliente
    void setDest(const char* ip, int port);

    // recibir un paquete
    bool receivePacket(char* buffer);

    // usado por el cliente ya que current_addr siempre sera el server
    bool sendPacket(const UDPPacket& p);

    // usado por el server ya que se puede colocar un cliente destino
    bool sendPacketTo(const UDPPacket& p, struct sockaddr_in dest);

    struct sockaddr_in getClientAddr();
};

#endif
