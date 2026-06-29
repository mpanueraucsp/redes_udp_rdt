#include "UDP.h"
#include "ACK.h"
#include <iostream>
#include <thread>
#include <fstream>
#include <chrono>
#include <mutex>

using namespace std;

class SlaveClient {
private:
    UDP udp;
    ACK ack;
    bool run = true;
    mutex mtx;

    int clientSeqNum = 1;
    const int buffSize = 500;
    int chunkSize = buffSize - 24;  // 500 - 24 bytes header

    UDPMsg msgBuffer;
    int rcvTotalPkts = 0;
    bool p = true; // print msgs

    // <layer, data>
    map<int, string> layerDataMap; // contiene todo el mensaje que se recibe del server para ser retornado y usado por slave.py 
    // <layer, isReady>
    map<int, bool> isLayerReady;


    // usado en el hilo de rcvServerThread para los fragementos en UDPMsg de forma que al terminar de recibir todos los fragmentos pueda usar UDPMsg para construr todo el mensaje en un string
    void rcvMasterDataFragments(StandarPacketData& req,  UDPPacket& packetTemp){
        // si no hay ningun paquete recibido entonces se registra el numero de paquetes que se espera recibir, asi tambien se agrega el primer fragmento al UDPMsg
        if (rcvTotalPkts == 0) {
            rcvTotalPkts = req.nPakects;
            if (p) cout << "\n[C]: primer paquete recibido\n\n";
        }
        msgBuffer.setFragment(req.actualPakect, packetTemp);

        // verifica si hay mas paquetes por recibir (msgBuffer es un mapa de fragmentos) 
        // si estan todos los paqutes de la capa se arma el mensaje completo en el string y se guarda en layerDataMap
        if (msgBuffer.getSize() == rcvTotalPkts && rcvTotalPkts > 0) {
            string allDataServerRcv = "";            
            for (int i = 1; i <= rcvTotalPkts; i++) {
                UDPPacket fragment = msgBuffer.UDPmsg[i];
                StandarPacketData fragData = unpackUDPData(fragment);
                allDataServerRcv += fragData.fileData;
            }

            // este lock es para que primero se guarde la informacion y luego se pueda extraer en receiveData para ser enviada al server
            mtx.lock();
            layerDataMap[req.layer] = allDataServerRcv;
            isLayerReady[req.layer] = true;
            mtx.unlock();

            if (p) cout << "\n[C]: paquetes guardados\n";
            
            // se limpia el UDPmsg puesto que ya guardamos el mensaje completo en layerDataMap, de esa forma se puede empezar a recibir otro mensaje del server
            msgBuffer.UDPmsg.clear();
            rcvTotalPkts = 0;
        }
    }

    // thread dispach para estar constantemente leyendo los paquetes que se reciben
    void rcvServerThread() {
        char buffer[buffSize];
        while (run) {
            memset(buffer, 0, sizeof(buffer));
            if (ack.receivePacket(buffer)) {
                UDPPacket packet; 
                packet.loadFromBuffer(buffer);

                // desarmar paquete (unpackUDPData) para saber sus metadatos
                // - se guarda en un temp (packetTemp) porque al desempaquetar un paquete se mueve el idx de lectura entonces guardamos un paquete auxiliar sin tratar para leerlo en rcvMasterDataFragments
                UDPPacket packetTemp = packet; 
                StandarPacketData req = unpackUDPData(packet);

                if (req.type == "1") {
                    rcvMasterDataFragments(req, packetTemp);
                }
            }
        }
    }

public:
    SlaveClient() : ack(&udp) {}
    ~SlaveClient() {run = false;}

    void start(string ip, int port) {
        udp.setDest(ip.c_str(), port);
        thread t(&SlaveClient::rcvServerThread, this);
        t.detach();
        
        // enviar registro al server para poder recibir el lote
        StandarPacketData registerPacket;
        registerPacket.type = "0"; 

        while (!ack.sendPacket(packDataToUDP(registerPacket))) {
            if (p) cout << "[Error] Server not conected. start reloging...\n";
        }
        if (p) cout << "[C]: Paquete de registro aceptado por server\n";

    }

    // para retornar la capa al slave.py 
    string receiveData(int layer) { 
        // veriica que la capa del server se termino de recibir
        while (!isLayerReady[layer]) continue;
        
        // mutex para obtener la capa de forma segura. En rcvMasterDataFragments (thread), se llena la informacion de layerDataMap[layer] por lo que se debe hacer ese mutex para que primero se llene y despues se pueda extraer su informacion y pasar a borrar esa capa de layerDataMap, liberando memoria
        mtx.lock();
        isLayerReady[layer] = false; 
        string data = layerDataMap[layer];
        layerDataMap.erase(layer); // liberando memoria
        mtx.unlock();
        return data; 
    }
    
    // para enviar las capas 
    // fileBytes = toda la informacion de una capa 
    // para que el mensaje se pueda fragmentar y lo envia al server
    void sendData(string fileBytes, int layer) {       
        int totalSize = fileBytes.size();
        int totalPackets = (totalSize + chunkSize - 1) / chunkSize;
        int bytesSend = 0;
        int packetNum = 1;

        while (bytesSend < totalSize) {  // enviar fragmentos
            int curChunkSize = min(chunkSize, totalSize - bytesSend); // para que el ultimo paquete sepa cuantos bytes debe ocupar en el buffer
            
            StandarPacketData pkt;
            pkt.type = "1";
            pkt.seqNum = clientSeqNum++;
            pkt.nPakects = totalPackets;
            pkt.actualPakect = packetNum;
            pkt.layer = layer;
            pkt.fileData = fileBytes.substr(bytesSend, curChunkSize);

            // enviar fragmento y validar si el RDT pudo enviar el fragmento, sino lo reenvia
            while (!ack.sendPacket(packDataToUDP(pkt))){
                if (p) cout << "\n[C ERROR] MaxRetry NumPkt " << packetNum << " REINTENTANDO RENVIO\n";
            }

            bytesSend += curChunkSize;
            packetNum++;
            if (clientSeqNum > 9999) clientSeqNum = 1;  // control del seqNum para que no sobrepase los 4 bytes maximos que puede ocupar
        }
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
