#include "UDP.h"
#include "ACK.h"
#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>

using namespace std;

struct TransferClient {
    UDPMsg msgBuffer;
    int rcvTotalPkts = 0;   
};

class MasterServer {
private:
    UDP udp;
    ACK ack;
    bool run = true;
    mutex mtx; 
    bool p = true; // print msgs

    vector<struct sockaddr_in> clientList;

    // <ip:port, Idx client[0,1,...]>
    map<string, int> clientIdMap; // id idx de clientes
    // layer, <Idx client, data>
    map<int, map<int, string>> layerDataMap; // guardar capas segun el cliente
    // <ip:port, TransferClient>
    map<string, TransferClient> slaveTransfers; // gestionar los fragmentos que se van recibiendo antes de armarlos para tener el msg completo

    atomic<int> serverSeqNum{1}; // para que los hilos no sobreescriban al mismo tiempo el seqNum del server

    const int buffSize = 500;
    int chunkSize = buffSize - 24;  // 500 - 24 bytes header


    string getSenderId(struct sockaddr_in addr) {
        return string(inet_ntoa(addr.sin_addr)) + "_" + to_string(ntohs(addr.sin_port));
    }

    // usado en el hilo de rcvClientThread para los fragementos en UDPMsg de forma que al terminar de recibir todos los fragmentos pueda usar UDPMsg para construr todo el mensaje en un string
    void rcvSlaveDataFragments( StandarPacketData& req,  UDPPacket& packetTemp, struct sockaddr_in sender) {
        string senderId = getSenderId(sender);
        TransferClient& tClient = slaveTransfers[senderId]; // <ip:port, TransferClient>

        // si no hay ningun paquete recibido entonces se registra el numero de paquetes que se espera recibir, asi tambien se agrega el primer fragmento al UDPMsg
        if (tClient.rcvTotalPkts == 0) {
            tClient.rcvTotalPkts = req.nPakects;
            if (p) cout << "\n[S]: Primer paquete recibido de " << senderId << "\n\n";
        }
        tClient.msgBuffer.setFragment(req.actualPakect, packetTemp);
        
        // verifica si hay mas paquetes por recibir (msgBuffer es un mapa de fragmentos) 
        // si estan todos los paqutes de la capa se arma el mensaje completo en el string y se guarda en layerDataMap
        if (tClient.msgBuffer.getSize() == tClient.rcvTotalPkts && tClient.rcvTotalPkts > 0) {
            string allData = "";
            // arma el mesaje en allData
            for (int i = 1; i <= tClient.rcvTotalPkts; i++) {
                UDPPacket fragment = tClient.msgBuffer.UDPmsg[i];
                StandarPacketData fragData = unpackUDPData(fragment);
                allData += fragData.fileData;
            }

            // este lock es para que primero se guarde la informacion y luego se pueda extraer en receiveAvgLayer para realizar las sumas de capas de los clientes
            mtx.lock(); 
            int clientId = clientIdMap[senderId];
            layerDataMap[req.layer][clientId] = allData;
            mtx.unlock();

            if (p) cout << "[S]: paquetes " << req.layer << " de " << senderId << " ID " << clientId << " guardado\n";

            // se limpia el UDPmsg puesto que ya guardamos el mensaje completo en layerDataMap, de esa forma se puede empezar a recibir otro mensaje del cliente
            tClient.msgBuffer.UDPmsg.clear();
            tClient.rcvTotalPkts = 0;
        }
    }

    // thread dispach para estar constantemente leyendo los paquetes que se reciben
    void rcvClientThread() {
        char buffer[buffSize];
        while (run) {
            memset(buffer, 0, sizeof(buffer));
            if (ack.receivePacket(buffer)) {
                UDPPacket packet; 
                packet.loadFromBuffer(buffer);
                
                // desarmar paquete (unpackUDPData) para saber sus metadatos
                // - se guarda en un temp (packetTemp) porque al desempaquetar un paquete se mueve el idx de lectura entonces guardamos un paquete auxiliar sin tratar para leerlo en rcvSlaveDataFragments
                UDPPacket packetTemp = packet;
                struct sockaddr_in sender = udp.getClientAddr();
                StandarPacketData req = unpackUDPData(packet);

                if (req.type == "0") { // registrar cliente
                    string senderId = getSenderId(sender);
                    // verifica que no exista el cliente para guarlarlo
                    if (clientIdMap.count(senderId) == 0) { 
                        // se guarda el idx del cliente segun el tamano de la lista. 
                        // - Se guarda el idx para tener un orden en el map (clientIdMap) y poder hacer la suma de las capas en receiveAvgLayer sin problemas por la suma de floats en distinto orden. 
                        // - el problema de no guardar un orden idx, es que si cada vez se suma las capas en orden distinto de los clientes (ya que el primero en terminar de recibir sus capa es el primero de la lista), es que la suma de floats da un resultado diferente segun su orden de suma. dando resultados distintos de train_loss en la NN   
                        int clientId = clientList.size(); 
                        clientIdMap[senderId] = clientId;
                        clientList.push_back(sender);
                        if (p) cout << "\n[S]: Slave " << senderId << " ID " << clientId << " registrado\n";
                    }
                }
                else if (req.type == "1") {
                    rcvSlaveDataFragments(req, packetTemp, sender);
                }
            }
        }
    }

    // para que el mensaje se pueda fragmentar y lo envia a un cliente 
    void sendDataToSlave(string fileBytes, int layer, struct sockaddr_in slaveAddr) {
        int totalSize = fileBytes.size();
        int totalPackets = (totalSize + chunkSize - 1) / chunkSize;
        int bytesSent = 0;
        int packetNum = 1;

        while (bytesSent < totalSize) { // enviar fragmentos
            int curChunkSize = min(chunkSize, totalSize - bytesSent); // para que el ultimo paquete sepa cuantos bytes debe ocupar en el buffer
            
            // armar paquete fragmento
            StandarPacketData pkt;
            pkt.type = "1";
            pkt.seqNum = serverSeqNum++; // debe ser atomico para cada hilo
            pkt.nPakects = totalPackets;
            pkt.actualPakect = packetNum;
            pkt.layer = layer;
            pkt.fileData = fileBytes.substr(bytesSent, curChunkSize);
            
            // enviar fragmento y validar si el RDT pudo enviar el fragmento, sino lo reenvia
            while (!ack.sendPacketTo(packDataToUDP(pkt), slaveAddr)) {
                if (p) cout << "\n[S ERROR] MaxRetry NumPkt " << packetNum << " REINTENTANDO RENVIO\n";
            }
            
            bytesSent += curChunkSize;
            packetNum++;
            if (serverSeqNum > 9999) serverSeqNum = 1; // control del seqNum para que no sobrepase los 4 bytes maximos que puede ocupar
        }
    }

public:
    MasterServer() : ack(&udp) {}
    ~MasterServer() { run = false; }
    void start(int port) {
        udp.bindPort(port);
        thread t(&MasterServer::rcvClientThread, this);
        t.detach(); 
    }

    // para ser usado en .py y saber si ya estantodos los clientes conectados y mandarles los lotes
    int getClientsConectedSize() {
        return clientList.size();
    }
    
    // para retornar la capa promediara al master.py 
    string receiveAvgLayer(int layer, int numEsclavos) {
        // verifica que la capa de todos los esclavos se terminaron de recibir
        while ((int)layerDataMap[layer].size() < numEsclavos) 
            continue;

        if (p) cout << "\n[S]: Calculando media para capa " << layer << "\n";
        
        // mutex para calcular suma y limpiar las capas de forma segura. En rcvSlaveDataFragments (thread), se llena la informacion de layerDataMap[layer] por lo que se debe hacer ese mutex para que primero se llene y despues se pueda extraer su informacion con .begin()
        mtx.lock(); 
        // - La informacion se pasa por float en char, y los dos primeros idx son fila y columna de la matriz, por ello se hace el cast<int>
        // - Se usa punteros porque el <pybind11/numpy.h> es lo que nos da (los numeros floats seguidos) para parsear de float python a c++
        // - como todas las matrices de la misma capa de los clientes tienen el mismo tamano solo se estrae el primero para calcular cuantos numFloats tiene y realizar la suma
        auto it = layerDataMap[layer].begin(); // <layer, <Idx client, data>>
        int rows = *(int*)(it->second.data());
        int cols = *(int*)(it->second.data() + sizeof(int));
        int numFloats = rows * cols;
        vector<float> avg(numFloats, 0.0f);
        
        // obtener suma de las capas N de slaves 
        // - Hace cast<float> de todos los datos de la matriz menos los 2 primeros que son <int> de fila y columna.
        for (auto& e : layerDataMap[layer]) {
            float* floats = (float*)(e.second.data() + 2 * sizeof(int));
            for (int i = 0; i < numFloats; i++) avg[i] += floats[i];
        }
        layerDataMap[layer].clear(); // liberando memoria
        mtx.unlock();
        
        // calcular media
        for (int i = 0; i < numFloats; i++) avg[i] /= numEsclavos;

        // se vuelve a char para ser enviado al python
        string result;
        result.append((char*)&rows, sizeof(int));
        result.append((char*)&cols, sizeof(int));
        result.append((char*)avg.data(), numFloats * sizeof(float));
        
        return result;
    }
    
    // para enviar las capas 
    // fileBytes = toda la informacion de una capa 
    void sendData(string fileBytes, int layer) {        
         // threads para enviar un mensaje a todos los clientes y evitar esperas por su ACK o NACK
        vector<thread> senderThreads;
        for (auto& slaveAddr : clientList) {
            senderThreads.push_back(thread(&MasterServer::sendDataToSlave, this, fileBytes, layer, slaveAddr));
        }
        
        for (auto& t : senderThreads) {
            t.join();
        }
    }

    // para ser usado en .py para enviar lotes correspondientes 
    void sendDataToClientIdx(string fileBytes, int layer, int clientIndex){ 
        struct sockaddr_in slaveAddr = clientList[clientIndex];
        sendDataToSlave(fileBytes, layer, slaveAddr);
    }

    // para aplicar anomalias en el .py
    void setConfigTests(
        bool enableCorrupt, int corruptInterval, int corruptSequence, int maxCorrupt,
        bool enablePacketLoss, int lossInterval, int lossSequence, int maxLoss,
        bool enableAckLoss, int ackLossInterval, int ackLossSequence, int maxAckLoss
    ) {
        ack.setConfigTests(
            enableCorrupt, corruptInterval, corruptSequence, maxCorrupt,
            enablePacketLoss, lossInterval, lossSequence, maxLoss,
            enableAckLoss, ackLossInterval, ackLossSequence, maxAckLoss
        );
    }
};

