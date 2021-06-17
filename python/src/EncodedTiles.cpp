#if !USE_GPU

#include <boost/python.hpp>
#include <boost/python/to_python_converter.hpp>

#include "Operator.h"
#include "Rectangle.h"
#include "TileUtilities.h"
#include "TasmWrappers.h"
#include <experimental/filesystem>

using namespace boost::python;

void export_EncodedTileInformation() {
    class_<tasm::python::PythonEncodedTileInformation>("EncodedTileInformation", no_init)
        .def("max_object_width", &tasm::python::PythonEncodedTileInformation::maxObjectWidth)
        .def("max_object_height", &tasm::python::PythonEncodedTileInformation::maxObjectHeight)
        .def("scan", &tasm::python::PythonEncodedTileInformation::scan);

    class_<tasm::TileInformation>("TileInformation", no_init)
        .add_property("filename", &tasm::TileInformation::filenameString)
        .def_readonly("width", &tasm::TileInformation::width)
        .def_readonly("height", &tasm::TileInformation::height)
        .def_readonly("frames_to_read", &tasm::TileInformation::framesToRead)
        .def_readonly("frame_offset", &tasm::TileInformation::frameOffsetInFile)
        .def_readonly("tile_rect", &tasm::TileInformation::tileRect);

    class_<tasm::python::PythonTileAndRectangleInformation>("TileAndRectangleInformation", no_init)
        .def("tile_information", &tasm::python::PythonTileAndRectangleInformation::tileInformation, return_internal_reference<>())
        .def("rectangles", &tasm::python::PythonTileAndRectangleInformation::rectangles, return_internal_reference<>());

    class_<std::list<tasm::Rectangle>>("lrect")
        .def("__iter__", iterator<std::list<tasm::Rectangle>>());

    class_<std::vector<int>, std::shared_ptr<std::vector<int>>>("ivec")
            .def("__iter__", iterator<std::vector<int>>());

    class_<tasm::Rectangle>("Rectangle", no_init)
        .def_readonly("id", &tasm::Rectangle::id)
        .def_readonly("x", &tasm::Rectangle::x)
        .def_readonly("y", &tasm::Rectangle::y)
        .def_readonly("width", &tasm::Rectangle::width)
        .def_readonly("height", &tasm::Rectangle::height)
        .def("intersects", &tasm::Rectangle::intersects);

    class_<tasm::python::PythonOperator<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>>("TileAndRectangleInformationOperator", no_init)
        .def("is_complete", &tasm::python::PythonOperator<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>::isComplete)
        .def("next", &tasm::python::PythonOperator<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>::next);

    class_<tasm::python::PythonOptional<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>>("TileAndRectangleInformationOptional", no_init)
        .def("is_empty", &tasm::python::PythonOptional<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>::isEmpty)
        .def("value", &tasm::python::PythonOptional<tasm::python::PythonTileAndRectangleInformation, std::shared_ptr<tasm::TileAndRectangleInformation>>::value, return_value_policy<return_by_value>());
}

#endif // !USE_GPU
