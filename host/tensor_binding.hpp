#pragma once
#include <pybind11/pybind11.h>

namespace spire::host {
template<class T> T* tensor_pointer(const pybind11::object& tensor, const char* dtype, int device) {
    const auto torch = pybind11::module_::import("torch");
    const auto location = tensor.attr("device");
    SPIRE_ASSERT(tensor.attr("dtype").is(torch.attr(dtype)));
    SPIRE_ASSERT(tensor.attr("is_contiguous")().cast<bool>());
    SPIRE_ASSERT(tensor.attr("ndim").cast<int>() == 1);
    SPIRE_ASSERT(location.attr("type").cast<std::string>() == (device < 0 ? "cpu" : "cuda"));
    if (device >= 0) SPIRE_ASSERT(location.attr("index").cast<int>() == device);
    return reinterpret_cast<T*>(tensor.attr("data_ptr")().cast<uintptr_t>());
}
template<class Batch, class Step, class Reset, class Store> void tensor_methods(pybind11::class_<Batch>& cls, Step step, Reset reset, Store store) {
    namespace py = pybind11;
    cls.def("store_tensors", store);
    cls.def("step_tensors", [step](Batch& b, py::object choices, py::object active) {
        SPIRE_ASSERT(choices.attr("numel")().cast<int>() == b.size && active.attr("numel")().cast<int>() == b.size);
        step(b, choices, active);
    });
    cls.def("reset_tensors", [reset](Batch& b, py::object slots, py::object roots, py::object seeds,
                                     py::object offsets, py::object candidates) {
        SPIRE_ASSERT(slots.attr("numel")().cast<int>() == roots.attr("numel")().cast<int>());
        reset(b, slots, roots, seeds, offsets, candidates);
    });
}

template<class Cache, class Batch, class Capture, class Reset> void root_cache_methods(
    pybind11::module_& module, pybind11::class_<Batch>& cls, Capture capture, Reset reset) {
    namespace py = pybind11;
    py::class_<Cache>(module, "RootCache").def(py::init<int>()).def_readonly("size", &Cache::size)
        .def_property_readonly("bytes", [](const Cache& cache) {
            return static_cast<std::size_t>(cache.size) * (sizeof(Game) + sizeof(obs::History) + sizeof(uint32_t));
        });
    cls.def("cache_roots_tensors", [capture](Batch& b, Cache& cache, py::object slots, py::object roots) {
        SPIRE_ASSERT(slots.attr("numel")().cast<int>() == roots.attr("numel")().cast<int>());
        capture(b, cache, slots, roots);
    });
    cls.def("reset_cached_tensors", [reset](Batch& b, const Cache& cache, py::object slots, py::object roots) {
        SPIRE_ASSERT(slots.attr("numel")().cast<int>() == roots.attr("numel")().cast<int>());
        reset(b, cache, slots, roots);
    });
}
}
