#include "ACK.h"

// ==================== Metodos privados ====================

// para contruir un ACK o NACK
UDPPacket ACK::packACKToUDP(string type, int seqNum) {
    UDPPacket p;
    p.addMsg(type); // 1 byte type
    p.addSize(seqNum, 4); // 4 bytes + seqnum
    p.setPadding('#');
    return p;
}

// para leer un ACK o NACK
ACKPacketData ACK::unpackUDPACK(UDPPacket& p) {
    ACKPacketData d;
    d.type = p.readMsg(1);
    d.seqNum = p.readSize(4);
    return d;
}

string ACK::getSenderId(struct sockaddr_in addr) {
    return string(inet_ntoa(addr.sin_addr)) + ":" + to_string(ntohs(addr.sin_port));
}

// PLANTILLA PARA CONTROLAR LAS ANOMALIAS DE PAQUETES
// sirve para saber si es momento de aplicar una anomalia retornando true, si retorna falso no se aplica la animalia
bool ACK::checkAnomalia(
    bool enable, int maxLimit, int& totalApplied,
    int& counterSequence, int& counterInterval,
    int interval, int sequence
) {
    // verifica si la anomalia esta activada y si llego al maximo de  anomalias aplicadas
    if (!enable || totalApplied >= maxLimit) return false;

    // sequence = secuencia en que se aplica una anomalia tras otra anomalia de forma seguida, va decrementado. Al inicio counterSequence es = 0
    if (counterSequence > 0) {
        counterSequence--;
        totalApplied++;
        return true;
    }
    else {
        counterInterval++;  // esperar n pkt's ya enviados para saber su se debe aplicar la anomalia
        // Interval = intervalos de pkt's sin anomalias para poder aplicar la anomalia.
        if (counterInterval >= interval) {
            counterSequence = sequence - 1; // -1 porq aqui tambien  aplico la anomalia
            counterInterval = 0;
            totalApplied++;
            return true;
        }
    }
    return false;
}

// PACKET LOSS
bool ACK::checkPacketLoss() {
    return checkAnomalia(
        enablePacketLoss, maxLoss, totalLossApplied,
        counterLossSequence, counterLossInterval,
        lossInterval, lossSequence
    );
}

// CORRUPT
bool ACK::checkCorrupt() {
    return checkAnomalia(
        enableCorrupt, maxCorrupt, totalCorruptApplied,
        counterCorruptSequence, counterCorruptInterval,
        corruptInterval, corruptSequence
    );
}

// ACK LOSS TIMEOUT
bool ACK::checkAckLoss() {
    return checkAnomalia(
        enableAckLoss, maxAckLoss, totalAckLossApplied,
        counterAckLossSequence, counterAckLossInterval,
        ackLossInterval, ackLossSequence
    );
}

// metodo usado en sendPacketTo y evalua el TIMEOUT de la espera para recibir el ACK o NACK
// recuerda que el ACK o NACK es el seqNum del ultimo paquete recibido.
// cuando se recibe un ack o nack se cambia las variables (lastAckSeq, lastNackSeq) con el sequence number del ack o nack recibido, para compararlo con el seqNum del paquete que se ha enviado en sendPacketTo
bool ACK::timeoutFun(int seqNum, string clientId) {
    int waited = 0;
    while (waited < timeout) {
        // el uso de este lock se describe en el seqMtx de sendPacketTo y receivePacket
        // en resumen. Se debe setear primero el ack y nack en  receivePacket para luego extraer su contenido, por eso se usa mutex
        seqMtx.lock();
        int ack = lastAckSeqMap[clientId];
        int nack = lastNackSeqMap[clientId];
        seqMtx.unlock();

        if (ack == seqNum) { // se recibio un ACK
            if (printLogs) cout << "\n[ACK WAIT FROM SEND] True\n";
            return true;
        }
        if (nack == seqNum) { // se recibio un NACK
            if (printLogs) cout << "\n[NACK WAIT FROM SEND] TRUE\n";
            break;
        }

        if (waited < 100 && printRcv) cout << "," << flush; // imprime , para ver los tiempos de la anomalia ACK LOSSS o PAQUET LOSS
        this_thread::sleep_for(chrono::milliseconds(1));
        waited++;
    }
    return false;
}

// imprimer el pkt
void ACK::printHeader(UDPPacket p) {
    if (printRcv)
        p.printHeader( "\n[RCV Packet " +
            to_string(ntohs(udp->getClientAddr().sin_port)) + "] >>>");
}

// identificar PAQUETES DUPLICADOS y guardar el pkt en dupePackets caso no sea duplicado
bool ACK::isDuplicated(const string& clientId, int seqNum) {
    // Lista deque de seqNum del cliente
    deque<int>& dupClient = dupePackets[clientId];

    // identificar PAQUETES DUPLICADOS
    // si es duplicado entonces se debe retornar true para que no se vuelva a guardar en la lista deque.
    if ( find(dupClient.begin(), dupClient.end(), seqNum)
        != dupClient.end())
    {
        cout << "\n[ == ACK == ] PAQUETE DUPLICADO SeqNum: " << seqNum << "\n";
        return true;
    }

    // guardar el seqNum del paquete del cliente para analizar duplicados futuros y si la lista ya supera los 100 seqNum entonces se elimina el mas antiguo para no saturar la memoria
    dupClient.push_back(seqNum);
    if (dupClient.size() > 100) dupClient.pop_front();
    return false;
}

// ==================== Metodos publicos ====================

ACK::ACK(UDP* u) {
    udp = u;
}

bool ACK::sendPacket(const UDPPacket& p) {
    return sendPacketTo(p, udp->getClientAddr(), false);
}

// metodo usado para enviar paquetes y verificar el ACK o NACK recibido de receivePacket. Lo usa server y client cpp
// tambien valida el timeout y se aplica la anomalia CORRUPT y PAQUET LOSS
bool ACK::sendPacketTo(const UDPPacket& p, struct sockaddr_in dest, bool useDest) {
    // se lee el seqNum del pkt
    string seqStr = p.getBuffer().substr(1, 4);
    int seqNum = stoi(seqStr);

    string clientId = getSenderId(dest);

    int actualRetry = 0;

    // enviar el paquete con reintentos si hubo un NACK o TIMEOUT
    while (actualRetry < maxRetry) {
        // se usa lock
        // Normalmente primero se envia el paquete y luego se debe recibir el ACK. Entonces porque aca se usa un lock, si aun no envio el paqute?:
        // - Por si ocurre un timeout de un ACK que aun no llega, esto quiere decir que aca me encuentro reintentado el reenvio del paquete, por lo que debo evitar un seteo de "-1" colisionado por el seqNum en "receivePacket" del ACK tardon, uno debe aplicarse despues del otro.
        // - Bueno se puede quitar y no hay problema, pero ayuda a mitigar los  TIMEOUT en timeoutFun por si primero se setea -1 y despues el seqNum. Ya que si primero se seta seqNum y despues el -1 la validaciones en timeoutFun (ack == seqNum) seria falsa y otravez un reintento.
        seqMtx.lock();
        lastAckSeqMap[clientId] = -1;
        lastNackSeqMap[clientId] = -1;
        seqMtx.unlock();

        bool applyLose = checkPacketLoss();
        bool applyCorrupt = applyLose ? false : checkCorrupt();

        if (applyLose) { // aplicar paquete perdido
            cout << "\n[ == APPLY PACKETLOSS == ]\n";
        }
        else if (applyCorrupt) { // aplicar paquete corrupto
            cout << "\n[ == APPLY CORRUPT == ]\n";
            UDPPacket corruptPacket = p;
            corruptPacket.setCorruptBuffer(corruptIdxPkt, '9'); // cambia el byte corruptIdxPkt del pkt por un '9'

            if (useDest) udp->sendPacketTo(corruptPacket, dest);
            else udp->sendPacket(corruptPacket);
        }
        else { // paqutes normales
            if (useDest) udp->sendPacketTo(p, dest); // enviar al cliente
            else udp->sendPacket(p); // enviar al server
        }

        // TIMEOUT
        // si es true entonces se recibo un ACK, si es NACK o TIMEPUT da false y se debe hacer un reintento de envio del pkt
        if (timeoutFun(seqNum, clientId)) return true;

        actualRetry++;
        cout << "\n[ == FAIL to SEND == ] Timeout o NACK. seqNum: " << seqNum << " - Intento: " << actualRetry << "/" <<maxRetry<<"\n";
    }
    return false;
}

// metodo usado para recibir paquetes y verificar si es ACK, NACK o un paquete normal. Lo usa server y client cpp
// tambien valida checkSum, duplicados y se aplica la anomalia TIMEOUT ACK LOSS
bool ACK::receivePacket(char* buffer) {
    // recibimos el paqute del UDP
    if (udp->receivePacket(buffer)) {

        UDPPacket temp;
        temp.loadFromBuffer(buffer);

        printHeader(temp); // imprime paquete

        // desarma el paquete para leer sus metadatos type y seqNum
        ACKPacketData ackData = unpackUDPACK(temp);
        string type = ackData.type;
        int seqNum = ackData.seqNum;
        string clientId = getSenderId(udp->getClientAddr());

        // ANALIZAR PAQUETE
        if (type == "2") { // ACK recibido
            // usamos el lock puesto que en sendPacketTo y timeoutFun lo usan para validar el ack y nack.
            // se usa porque cuando se envia un paquete se debe esperar el ACK y en las validaciones de "timeoutFun" en sendPacketTo se hace lectura de  lastAckSeqMap[clientId], entonces primero se debe setear el seqNum para luego leerlo.
            seqMtx.lock();
            lastAckSeqMap[clientId] = seqNum;
            seqMtx.unlock();
            if (printLogs) cout << "\n[ACK to RCV] OK\n";
            return false;
        }
        else if (type == "3") { // NACK recibido
            // lo mismo con "lastAckSeqMap"
            seqMtx.lock();
            lastNackSeqMap[clientId] = seqNum;
            seqMtx.unlock();
            if (printLogs) cout << "\n[NACK to RCV] OK\n";
            return false;
        }
        else if (type == "1" || type == "0") { // paquete recibido
            temp.loadFromBuffer(buffer); // seteamos el buffer en temp

            // ACK LOSS TIMEOUT, verifica si se aplica
            bool applyAckLoss = checkAckLoss();

            // si el checksum es verdadero entonces se debe enviar el ACK de respuesta para confirmar que se recibio de forma exitosa el pkt
            // Una ves enviado el ACK se debe validar si dicho pkt recibido es un duplicado
            if (temp.verifyChecksum()) { // CHECK SUM
                struct sockaddr_in clientAddr = udp->getClientAddr();
                string clientId = getSenderId(clientAddr);

                UDPPacket ack = packACKToUDP("2", seqNum);

                // ACK
                if (applyAckLoss) {  // LOSS TIMEOUT
                    cout << "\n[ == APPLY ACK LOSS == ] SeqNum " << seqNum << "\n";
                }
                else { // SEND ACK
                    if (printLogs) cout << "\n[ == RDT == ] CHECKSUM OK SEND ACK\n";
                    udp->sendPacketTo(ack, clientAddr);
                }

                // identificar PAQUETES DUPLICADOS y guardar el pkt en dupePackets caso no sea duplicado
                if (isDuplicated(clientId, seqNum)) return false;
                return true;
            }
            else {
                // si el checksum fallo entonces se debe enviar el NACK
                UDPPacket nack = packACKToUDP("3", seqNum);

                // NACK
                if (applyAckLoss) { /// LOSS TIMEOUT
                    cout << "\n[ == APPLY NACK LOSS == ] SeqNum " << seqNum << "\n";
                }
                else {  // SEND NACK
                    cout << "\n[ == RDT == ] CHECKSUM FAILD SEND NACK\n";
                    udp->sendPacketTo(nack, udp->getClientAddr());
                }

                return false;
            }
        }
    }
    return false;
}

// setear las anomalias, este metodo es usado por cliente y server cpp para que sea llamado desde master y slaves.py
void ACK::setConfigTests(bool ec, int ci, int cs, int mc,
                    bool epl, int li, int ls, int ml,
                    bool eal, int ali, int als, int mal) {
    // corrupt
    enableCorrupt = ec;
    corruptInterval = ci;
    corruptSequence = cs;
    maxCorrupt = mc;

    // paquet loss
    enablePacketLoss = epl;
    lossInterval = li;
    lossSequence = ls;
    maxLoss = ml;

    // timeout ack loss
    enableAckLoss = eal;
    ackLossInterval = ali;
    ackLossSequence = als;
    maxAckLoss = mal;

    // resetear contadores y totales
    counterCorruptInterval = 0;
    counterCorruptSequence = 0;
    counterLossInterval = 0;
    counterLossSequence = 0;
    counterAckLossInterval = 0;
    counterAckLossSequence = 0;

    totalCorruptApplied = 0;
    totalLossApplied = 0;
    totalAckLossApplied = 0;
}
