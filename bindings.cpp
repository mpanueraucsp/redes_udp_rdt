#include <pybind11/pybind11.h>

#include "s.cpp" 
#include "c.cpp"

namespace py = pybind11;

PYBIND11_MODULE(UDPBind, m) {
    m.doc() = "Server.cpp + Client.cpp + UDP + RDT";

    py::class_<MasterServer>(m, "MasterServer")


    py::class_<SlaveClient>(m, "SlaveClient")

}