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

void readIn(const char* msg, string& buffer) {
    cout << msg << flush;
    getline(cin, buffer);
}

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
    UDPPacket() { 
        buf.assign(sizeBuf, padding); // setear padding
        actualSizeBuf = 0; 
        actualReadBuf = 0;
    }

    // carga el char del buffer recibido en receivePacket de UDP
    void loadFromBuffer( char* data) {
        buf.assign(data, sizeBuf);
        actualReadBuf = 0; 
    }
    
    // setear padding desde el idx de tamano aplicado en el buffer
    void setPadding(char p) {
        int tempSizeBuf = actualSizeBuf;
        for (int i = tempSizeBuf; i < sizeBuf; i++) 
            buf[i] = p;        
    }

    // ARMAR/AGREGAR AL BUFFER
    // agrega un tamano del msg y despues el msg
    // ej. msg = "Holas", bytesSize = 3 => [...005Holas]
    void addSizeAndMsg( string& msg, int bytesSize) {
        addSize(msg.size(), bytesSize); // [...005]
        addMsg(msg); // [...Holas]
    }

    // agrega el tamano en un formato
    // ej. size = 11, bytesSize = 4 => [...0011]
    void addSize(int size, int bytesSize) {
        char formato[50]={0};
        snprintf(formato, sizeof(formato), "%%0%dd", bytesSize); // "%04d" 
        char sizeStr[50]={0};
        snprintf(sizeStr, sizeof(sizeStr), formato, size); // "0011"
        for (int i = 0; i < bytesSize; ++i) 
            buf[actualSizeBuf++] = sizeStr[i];
    }

    // agrega un msg 
    // ej. msg = Patos => [...Patos]
    void addMsg( string& msg) {
        for(size_t i = 0; i < msg.size(); ++i)
            buf[actualSizeBuf++] = msg[i];
    }

    // sobreacarga, agrega un msg char
    // ej. c = '4' => [...4]
    void addMsg(char c) {
        buf[actualSizeBuf++] = c;
    }

    // LEER EL BUFFER
    // leer un tamano y segun ese tamano lee y retorna el msg, por referencia retorna el tamano 
    // ej. bytesSize = 4, numberInBytesSize = 0, buffer = [0005Holas...] =>
    //      numberInBytesSize = 5
    //      return "Holas"
    string readSizeAndMsg(int bytesSize, int& numberInBytesSize) {
        numberInBytesSize = readSize(bytesSize);
        return readMsg(numberInBytesSize);
    }
    
    // lee un tamano y retorna el numero contenido
    // ej. bytesSize = 5, buffer = [00041...] =>  
    //      return 41
    int readSize(int bytesSize) {
        string sizeStr = buf.substr(actualReadBuf, bytesSize);
        actualReadBuf += bytesSize;         
        return stoi(sizeStr);
    }

    // lee un msg segun el numero de bytes y retorna el msg
    // ej. bytesSize = 3, buff [Sir...] => 
    //      return "Sir"
    string readMsg(int bytesSize) {
        string msg = buf.substr(actualReadBuf, bytesSize);
        actualReadBuf += bytesSize;
        return msg;
    }

    // calcula el checksum desde un idx en el buff hasta el final del buff segun el sizeBuffer 
    // lo que hace es sumar los char de todo el buffer, y cada char se le multiplica su posicion (i - idxBegin + 1) para tener un checksum mas seguro
    // al final se aplica el modul 94 + 32 ya que hay 94 caracteres normales despues del ASCII 32, y se hace un cast<char>  
    char calCheckSum(int idxBegin)  {
        long long sum = 0;
        for (int i = idxBegin; i < sizeBuf; i++) {
            sum += (unsigned char)buf[i] * (i - idxBegin + 1);
        }
        sum = (sum % modul) + 32;
        return (char)(sum);
    }

    // verifica el checksum sea correcto una vez cargo el buffer recibido
    bool verifyChecksum()  { 
        return calCheckSum(checksumIdxBegin) == buf[idxSetChecksum]; 
    }

    // setea el checksum en su posicion definida idxSetChecksum
    void setChecksum() { 
        buf[idxSetChecksum] = calCheckSum(checksumIdxBegin); 
    }

    // imprime el buffer sin formato del header
    void printBuf() const {
        cout << buf << endl;
    }

    // imprime el header del buffer con un formato para poder leerlo comodamente
    void printHeader(const string tag = "\n", bool printFormate = printHederFormate) const {
        if (!printPaquetSend) return;
        if (!printFormate) {
            printBuf();
            return;
        }
        int pos = 0;
        string type = buf.substr(pos, 1); 
        pos += 1;
        string seqNum = buf.substr(pos, 4); 
        pos += 4;
        string checkSumSize = buf.substr(pos, 4); 
        pos += 4;
        string checkSumMsg = buf.substr(pos, 1); 
        pos += 1;
        string nPackets = buf.substr(pos, 4); 
        pos += 4;
        string actualPacket = buf.substr(pos, 4); 
        pos += 4;
        string layer = buf.substr(pos, 2); 
        pos += 2;
        string fileDataSize = buf.substr(pos, 4); 
        pos += 4;
        string fileData;
        if ( dataFileBytesPrint != 0) 
            fileData = buf.substr(pos, dataFileBytesPrint); 
        else fileData = buf.substr(pos); 

        cout << tag
            << type << "-" 
            << seqNum << "-" 
            << checkSumSize << "-" 
            << checkSumMsg << "-" 
            << nPackets << "-" 
            << actualPacket << "-" 
            << layer << "-" 
            << fileDataSize << "-" 
            << fileData << endl;
    }

    // retorna el buffer
    const string& getBuffer() const { return buf; }

    // para setear el byte idx corrupto
    void setCorruptBuffer(int idx, char val) { buf[idx] = val; }
    int getSize()  { return sizeBuf; }
};

// clase para poder gestionar los fragmentos de un mensaje
class UDPMsg {
public:
    map<int, UDPPacket> UDPmsg;    
    UDPMsg() {
        UDPmsg.clear();
    }
    void setFragment(int orden, UDPPacket fragment){ 
        UDPmsg[orden] = fragment; 
    }
    int getSize(){
        return UDPmsg.size();
    }
};

// Para armar un paqute segun un protocolo 
UDPPacket packDataToUDP( StandarPacketData& d) {
    UDPPacket p;
    p.addMsg(d.type); // 1 Byte
    p.addSize(d.seqNum, 4); // 4 Bytes
    p.addSizeAndMsg(d.checkSum, 4); // 5 Bytes = 4 Bytes + 1 Bytes
    p.addSize(d.nPakects, 4); // 4 Bytes 
    p.addSize(d.actualPakect, 4);  // 4 Bytes 
    p.addSize(d.layer, 2); // 2 Bytes
    p.addSizeAndMsg(d.fileData, 4); // 4 Bytes size + n data    
    p.setChecksum(); 
    return p;
}

// Para desarmar un paqute segun un protocolo 
StandarPacketData unpackUDPData(UDPPacket& p) {
    StandarPacketData d;
    int len; 
    
    d.type = p.readMsg(1);
    d.seqNum = p.readSize(4);
    d.checkSum = p.readSizeAndMsg(4, len); 
    d.nPakects = p.readSize(4);
    d.actualPakect = p.readSize(4);
    d.layer = p.readSize(2);
    d.fileData = p.readSizeAndMsg(4, len);
    
    return d;
}

// clase UDP para realizar la conexion del server y cliente, asi tambien permitirles enviar y recibir datos
class UDP {
private:
    int fd;
    struct sockaddr_in current_addr;
    socklen_t addr_len;
    int sizeBuf = 500;

public:
    UDP() {
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        addr_len = sizeof(current_addr);
        memset(&current_addr, 0, sizeof(current_addr));
    }
    ~UDP() { close(fd); }

    // para que se conecte el server
    bool bindPort(int port) {
        struct sockaddr_in adr = {AF_INET, htons(port), INADDR_ANY};
        return bind(fd, (struct sockaddr*)&adr, sizeof(adr)) == 0;
    }

    // para que se conecte el cliente
    void setDest(const char* ip, int port) {
        current_addr.sin_family = AF_INET;
        current_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &current_addr.sin_addr);
    }

    // recibir un paquete
    bool receivePacket(char* buffer) {
        return recvfrom(fd, buffer, sizeBuf, 0, (struct sockaddr*)&current_addr, &addr_len) == sizeBuf;
    }

    // usado por el cliente ya que current_addr siempre sera el server
    bool sendPacket(const UDPPacket& p) {
        p.printHeader("\n[SEND] >>>");
        return sendto(fd, p.getBuffer().data(), sizeBuf, 0, (struct sockaddr*)&current_addr, addr_len) == sizeBuf;
    }
    
    // usado por el server ya que se puede colocar un cliente destino
    bool sendPacketTo(const UDPPacket& p, struct sockaddr_in dest) {
        p.printHeader("\n[SEND " + std::to_string(ntohs(dest.sin_port)) + "] >>>");
        return sendto(fd, p.getBuffer().data(), sizeBuf, 0, (struct sockaddr*)&dest, sizeof(dest)) == sizeBuf;
    }
    
    struct sockaddr_in getClientAddr() { return current_addr; }
};

#endif