//
// Created by zhenyus on 11/3/18.
//
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <simulation.h>

namespace py = pybind11;

PYBIND11_MODULE(webcachesim, m) {

    m.def("simulation", &simulation, R"pbdoc(
        Run simulation for 1 configuration
    )pbdoc");

#ifdef VERSION_INFO
m.attr("__version__") = VERSION_INFO;
#else
m.attr("__version__") = "dev";
#endif
}