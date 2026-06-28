#include "UDP.h"

void readIn(const char* msg, string& buffer) {
    cout << msg << flush;
    getline(cin, buffer);
}

// ==================== UDPPacket ====================

UDPPacket::UDPPacket() {
    buf.assign(sizeBuf, padding); // setear padding
    actualSizeBuf = 0;
    actualReadBuf = 0;
}

// carga el char del buffer recibido en receivePacket de UDP
void UDPPacket::loadFromBuffer(char* data) {
    buf.assign(data, sizeBuf);
    actualReadBuf = 0;
}

// setear padding desde el idx de tamano aplicado en el buffer
void UDPPacket::setPadding(char p) {
    int tempSizeBuf = actualSizeBuf;
    for (int i = tempSizeBuf; i < sizeBuf; i++)
        buf[i] = p;
}

// ARMAR/AGREGAR AL BUFFER
// agrega un tamano del msg y despues el msg
// ej. msg = "Holas", bytesSize = 3 => [...005Holas]
void UDPPacket::addSizeAndMsg(string& msg, int bytesSize) {
    addSize(msg.size(), bytesSize); // [...005]
    addMsg(msg); // [...Holas]
}

// agrega el tamano en un formato
// ej. size = 11, bytesSize = 4 => [...0011]
void UDPPacket::addSize(int size, int bytesSize) {
    char formato[50]={0};
    snprintf(formato, sizeof(formato), "%%0%dd", bytesSize); // "%04d"
    char sizeStr[50]={0};
    snprintf(sizeStr, sizeof(sizeStr), formato, size); // "0011"
    for (int i = 0; i < bytesSize; ++i)
        buf[actualSizeBuf++] = sizeStr[i];
}

// agrega un msg
// ej. msg = Patos => [...Patos]
void UDPPacket::addMsg(string& msg) {
    for(size_t i = 0; i < msg.size(); ++i)
        buf[actualSizeBuf++] = msg[i];
}

// sobreacarga, agrega un msg char
// ej. c = '4' => [...4]
void UDPPacket::addMsg(char c) {
    buf[actualSizeBuf++] = c;
}

// LEER EL BUFFER
// leer un tamano y segun ese tamano lee y retorna el msg, por referencia retorna el tamano
// ej. bytesSize = 4, numberInBytesSize = 0, buffer = [0005Holas...] =>
//      numberInBytesSize = 5
//      return "Holas"
string UDPPacket::readSizeAndMsg(int bytesSize, int& numberInBytesSize) {
    numberInBytesSize = readSize(bytesSize);
    return readMsg(numberInBytesSize);
}

// lee un tamano y retorna el numero contenido
// ej. bytesSize = 5, buffer = [00041...] =>
//      return 41
int UDPPacket::readSize(int bytesSize) {
    string sizeStr = buf.substr(actualReadBuf, bytesSize);
    actualReadBuf += bytesSize;
    return stoi(sizeStr);
}

// lee un msg segun el numero de bytes y retorna el msg
// ej. bytesSize = 3, buff [Sir...] =>
//      return "Sir"
string UDPPacket::readMsg(int bytesSize) {
    string msg = buf.substr(actualReadBuf, bytesSize);
    actualReadBuf += bytesSize;
    return msg;
}

// calcula el checksum desde un idx en el buff hasta el final del buff segun el sizeBuffer
// lo que hace es sumar los char de todo el buffer, y cada char se le multiplica su posicion (i - idxBegin + 1) para tener un checksum mas seguro
// al final se aplica el modul 94 + 32 ya que hay 94 caracteres normales despues del ASCII 32, y se hace un cast<char>
char UDPPacket::calCheckSum(int idxBegin) {
    long long sum = 0;
    for (int i = idxBegin; i < sizeBuf; i++) {
        sum += (unsigned char)buf[i] * (i - idxBegin + 1);
    }
    sum = (sum % modul) + 32;
    return (char)(sum);
}

// verifica el checksum sea correcto una vez cargo el buffer recibido
bool UDPPacket::verifyChecksum() {
    return calCheckSum(checksumIdxBegin) == buf[idxSetChecksum];
}

// setea el checksum en su posicion definida idxSetChecksum
void UDPPacket::setChecksum() {
    buf[idxSetChecksum] = calCheckSum(checksumIdxBegin);
}

// imprime el buffer sin formato del header
void UDPPacket::printBuf() const {
    cout << buf << endl;
}

// imprime el header del buffer con un formato para poder leerlo comodamente
void UDPPacket::printHeader(const string tag, bool printFormate) const {
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
const string& UDPPacket::getBuffer() const { return buf; }

// para setear el byte idx corrupto
void UDPPacket::setCorruptBuffer(int idx, char val) { buf[idx] = val; }
int UDPPacket::getSize() { return sizeBuf; }

// ==================== UDPMsg ====================

UDPMsg::UDPMsg() {
    UDPmsg.clear();
}

void UDPMsg::setFragment(int orden, UDPPacket fragment) {
    UDPmsg[orden] = fragment;
}

int UDPMsg::getSize() {
    return UDPmsg.size();
}

// ==================== Funciones libres ====================

// Para armar un paqute segun un protocolo
UDPPacket packDataToUDP(StandarPacketData& d) {
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

// ==================== UDP ====================

UDP::UDP() {
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    addr_len = sizeof(current_addr);
    memset(&current_addr, 0, sizeof(current_addr));
}

UDP::~UDP() { close(fd); }

// para que se conecte el server
bool UDP::bindPort(int port) {
    struct sockaddr_in adr = {AF_INET, htons(port), INADDR_ANY};
    return bind(fd, (struct sockaddr*)&adr, sizeof(adr)) == 0;
}

// para que se conecte el cliente
void UDP::setDest(const char* ip, int port) {
    current_addr.sin_family = AF_INET;
    current_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &current_addr.sin_addr);
}

// recibir un paquete
bool UDP::receivePacket(char* buffer) {
    return recvfrom(fd, buffer, sizeBuf, 0, (struct sockaddr*)&current_addr, &addr_len) == sizeBuf;
}

// usado por el cliente ya que current_addr siempre sera el server
bool UDP::sendPacket(const UDPPacket& p) {
    p.printHeader("\n[SEND] >>>");
    return sendto(fd, p.getBuffer().data(), sizeBuf, 0, (struct sockaddr*)&current_addr, addr_len) == sizeBuf;
}

// usado por el server ya que se puede colocar un cliente destino
bool UDP::sendPacketTo(const UDPPacket& p, struct sockaddr_in dest) {
    p.printHeader("\n[SEND " + std::to_string(ntohs(dest.sin_port)) + "] >>>");
    return sendto(fd, p.getBuffer().data(), sizeBuf, 0, (struct sockaddr*)&dest, sizeof(dest)) == sizeBuf;
}

struct sockaddr_in UDP::getClientAddr() { return current_addr; }
