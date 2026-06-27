#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "s.cpp" 
#include "c.cpp"

namespace py = pybind11;

PYBIND11_MODULE(UDPBind, m) {
    m.doc() = "Server.cpp + Client.cpp + UDP + RDT";

    py::class_<MasterServer>(m, "MasterServer")
        .def(py::init<>())
        .def("setConfigTests", &MasterServer::setConfigTests, "configurar anomalias")
        .def("start", &MasterServer::start, "iniciar server")
        .def("getClientsConectedSize", &MasterServer::getClientsConectedSize, "get cantidad de clientes coenctados")
        .def("sendCSV", &MasterServer::sendDataToClientIdx, "enviar informacion a un cliente en especifico (usado para el CSV de lotes)")
        .def("sendLayer", []
            (
                MasterServer& self, 
                py::array_t<float, py::array::c_style | py::array::forcecast> matrixLayer, 
                int layer
            ) {
            // .request para obtener el puntero de la matriz y obtener el tamano de las filas y columnas
            py::buffer_info info = matrixLayer.request();
            int rows = info.shape[0]; 
            int cols = info.shape[1];
            
            // int filas + int columnas + data float
            // se convierte a char para respetar el protocolo de string facilitando su envio por server <-> cliente cpp
            std::string payload;
            payload.append((char*)&rows, sizeof(int));
            payload.append((char*)&cols, sizeof(int));
            payload.append((char*)info.ptr, info.size * sizeof(float));
            
            self.sendData(payload, layer);
        }, "enviar informacion de pesos o bias a todos los clientes")
        
        .def("receiveAvgLayer", [](MasterServer& self, int layer, int numEsclavos) {
            std::string data = self.receiveAvgLayer(layer, numEsclavos);
            
            // como es un string debemos convertirlo a la matriz de floats y al  ser numeros en bytes en conseguitivo se hace el memcpy
            int rows = *(int*)(data.data());
            int cols = *(int*)(data.data() + sizeof(int));
            float* floats = (float*)(data.data() + 2 * sizeof(int));
            
            // se prepara "result" para pasar la matriz a python
            py::array_t<float> result({rows, cols});
            py::buffer_info buf = result.request();
            memcpy(buf.ptr, floats, rows * cols * sizeof(float));
            
            return result;
        }, "recibir la capa ya promediada de pesos o bias");
        

    py::class_<SlaveClient>(m, "SlaveClient")
        .def(py::init<>())
        .def("setConfigTests", &SlaveClient::setConfigTests, "configurar anomalias")
        .def("start", &SlaveClient::start, "inicia el cliente y conecta al servidor")
        .def("receiveCSV", &SlaveClient::receiveData, "recibir lote del server")
        
        .def("sendLayer", []
            (
                SlaveClient& self, 
                py::array_t<float, py::array::c_style | py::array::forcecast> matrix,
                int layer
            ) {
            // .request para obtener el puntero de la matriz y obtener el tamano de las filas y columnas
            py::buffer_info info = matrix.request();
            int rows = info.shape[0];
            int cols = info.shape[1];
            
            // int filas + int columnas + data float
            // se convierte a char para respetar el protocolo de string facilitando su envio por server <-> cliente cpp
            std::string payload;
            payload.append((char*)&rows, sizeof(int));
            payload.append((char*)&cols, sizeof(int));
            payload.append((char*)info.ptr, info.size * sizeof(float));
            
            self.sendData(payload, layer);
        }, "enviar pesos o bias de la capa al server")
        
        .def("receiveLayer", [](SlaveClient& self, int layer) {
            std::string data = self.receiveData(layer);
            
            // como es un string debemos convertirlo a la matriz de floats y al  ser numeros en bytes en conseguitivo se hace el memcpy
            int rows = *(int*)(data.data());
            int cols = *(int*)(data.data() + sizeof(int));
            float* floats = (float*)(data.data() + 2 * sizeof(int));
            
             // se prepara "result" para pasar la matriz a python
            py::array_t<float> result({rows, cols});
            py::buffer_info buf = result.request();
            memcpy(buf.ptr, floats, rows * cols * sizeof(float));
            
            return result;
        }, "recibir pesos o bias de la capa por el server");
}