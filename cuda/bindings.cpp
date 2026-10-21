#include "training_batch.hpp"
#include "host/tensor_binding.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;
using spire::cuda::TrainingBatch;
namespace obs = spire::obs;

struct DeviceArray {
    py::object owner;
    uintptr_t pointer;
    std::vector<py::ssize_t> shape;
    const char* typestr;
};
template<class T> static DeviceArray array(TrainingBatch& batch, spire::cuda::Buffer<T>& buffer,
                                          std::vector<py::ssize_t> shape, const char* typestr) {
    return {py::cast(&batch, py::return_value_policy::reference), reinterpret_cast<uintptr_t>(buffer.data), shape, typestr};
}

PYBIND11_MODULE(spire_gpu, module) {
    module.attr("PLAYED_CARDS_CAPACITY_FAULT") = spire::played_cards_capacity_fault;
    py::class_<DeviceArray>(module, "DeviceArray")
        .def_property_readonly("__cuda_array_interface__", [](const DeviceArray& a) {
            return py::dict("shape"_a=py::tuple(py::cast(a.shape)), "typestr"_a=a.typestr,
                            "data"_a=py::make_tuple(a.pointer, false), "version"_a=3);
        });
    auto cls = py::class_<TrainingBatch>(module, "TrainingBatch");
    using spire::host::tensor_pointer;
    using spire::cuda::RootCache;
    spire::host::root_cache_methods<RootCache>(module, cls,
        [](TrainingBatch& b, RootCache& cache, py::object slots, py::object roots) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", b.device);
            auto* r = tensor_pointer<int64_t>(roots, "int64", b.device);
            auto stream = py::module_::import("torch").attr("cuda").attr("current_stream")(b.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            b.cache_roots(cache, s, r, count, reinterpret_cast<cudaStream_t>(stream));
        },
        [](TrainingBatch& b, const RootCache& cache, py::object slots, py::object roots) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", b.device);
            auto* r = tensor_pointer<int64_t>(roots, "int64", b.device);
            auto stream = py::module_::import("torch").attr("cuda").attr("current_stream")(b.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            b.reset_cached(cache, s, r, count, reinterpret_cast<cudaStream_t>(stream));
        });
    spire::host::tensor_methods(cls,
        [](TrainingBatch& b, py::object choices, py::object active) {
            auto* c = tensor_pointer<int32_t>(choices, "int32", b.device);
            auto* a = tensor_pointer<uint8_t>(active, "uint8", b.device);
            auto stream = py::module_::import("torch").attr("cuda").attr("current_stream")(b.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            b.step_tensors(c, a, reinterpret_cast<cudaStream_t>(stream));
        },
        [](TrainingBatch& b, py::object slots, py::object roots, py::object seeds,
           py::object offsets, py::object candidates) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", b.device);
            auto* r = tensor_pointer<int64_t>(roots, "int64", b.device);
            auto* e = tensor_pointer<int64_t>(seeds, "int64", b.device);
            auto* o = tensor_pointer<int64_t>(offsets, "int64", b.device);
            auto* c = tensor_pointer<int32_t>(candidates, "int32", b.device);
            auto stream = py::module_::import("torch").attr("cuda").attr("current_stream")(b.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            b.reset_tensors(s, r, count, e, o, c, reinterpret_cast<cudaStream_t>(stream));
        },
        [](TrainingBatch& b, py::object slots, py::object data, py::object offsets) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", b.device);
            auto* d = tensor_pointer<uint8_t>(data, "uint8", b.device);
            auto* o = tensor_pointer<int64_t>(offsets, "int64", b.device);
            auto stream = py::module_::import("torch").attr("cuda").attr("current_stream")(b.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            b.store_tensors(s, count, d, o, reinterpret_cast<cudaStream_t>(stream));
        });
    cls.def(py::init<int, uint64_t, const std::vector<int64_t>&>(), "size"_a, "decision_cap"_a, "seeds"_a)
        .def("step", [](TrainingBatch& batch, py::object selected) {
            const auto torch = py::module_::import("torch");
            if (!selected.attr("is_cuda").cast<bool>() || !selected.attr("is_contiguous")().cast<bool>() ||
                !selected.attr("dtype").is(torch.attr("int32")) || selected.attr("ndim").cast<int>() != 1 ||
                selected.attr("numel")().cast<int>() != batch.size ||
                selected.attr("device").attr("index").cast<int>() != batch.device)
                throw py::value_error("Expected contiguous CUDA int32 candidate_indices[size] on the batch device");
            const auto pointer = selected.attr("data_ptr")().cast<uintptr_t>();
            const auto stream = torch.attr("cuda").attr("current_stream")(batch.device).attr("cuda_stream").cast<uintptr_t>();
            py::gil_scoped_release release;
            batch.step(reinterpret_cast<const int32_t*>(pointer), reinterpret_cast<cudaStream_t>(stream));
        }, "candidate_indices"_a)
        .def("step_slots", &TrainingBatch::step_slots, "slots"_a, "candidate_indices"_a, py::call_guard<py::gil_scoped_release>())
        .def("reset_to", &TrainingBatch::reset_to, "slot"_a, "seed"_a, "candidates"_a, py::call_guard<py::gil_scoped_release>())
        .def("reset_to_many", &TrainingBatch::reset_to_many, "slots"_a, "seeds"_a, "candidates"_a, py::call_guard<py::gil_scoped_release>())
        .def("clone", &TrainingBatch::clone, "source"_a, "target"_a, py::call_guard<py::gil_scoped_release>())
        .def("extend_seeds", &TrainingBatch::extend_seeds, "seeds"_a, py::call_guard<py::gil_scoped_release>())
        .def_readonly("size", &TrainingBatch::size)
        .def_readonly("device", &TrainingBatch::device);
    cls.def_property_readonly("tags", [](TrainingBatch& b) { return array(b, b.tags, {b.size, obs::tag_rows, obs::entity_capacity}, "|u1"); })
        .def_property_readonly("ids", [](TrainingBatch& b) { return array(b, b.ids, {b.size, obs::id_rows, obs::entity_capacity}, "<i2"); })
        .def_property_readonly("values", [](TrainingBatch& b) { return array(b, b.values, {b.size, obs::value_rows, obs::entity_capacity}, "<i2"); })
        .def_property_readonly("candidate_kinds", [](TrainingBatch& b) { return array(b, b.candidate_kinds, {b.size, obs::candidate_capacity}, "|u1"); })
        .def_property_readonly("candidate_refs", [](TrainingBatch& b) { return array(b, b.candidate_refs, {b.size, obs::candidate_rows, obs::candidate_capacity}, "<i2"); })
        .def_property_readonly("entity_mask", [](TrainingBatch& b) { return array(b, b.entity_mask, {b.size, obs::entity_mask_bytes}, "|u1"); })
        .def_property_readonly("candidate_mask", [](TrainingBatch& b) { return array(b, b.candidate_mask, {b.size, obs::candidate_mask_bytes}, "|u1"); })
        .def_property_readonly("entity_counts", [](TrainingBatch& b) { return array(b, b.entity_counts, {b.size}, "<i4"); })
        .def_property_readonly("candidate_counts", [](TrainingBatch& b) { return array(b, b.candidate_counts, {b.size}, "<i4"); })
        .def_property_readonly("rewards", [](TrainingBatch& b) { return array(b, b.rewards, {b.size}, "<f4"); })
        .def_property_readonly("done", [](TrainingBatch& b) { return array(b, b.done, {b.size}, "|u1"); })
        .def_property_readonly("truncated", [](TrainingBatch& b) { return array(b, b.truncated, {b.size}, "|u1"); })
        .def_property_readonly("reset_mask", [](TrainingBatch& b) { return array(b, b.reset_mask, {b.size}, "|u1"); })
        .def_property_readonly("advanced", [](TrainingBatch& b) { return array(b, b.advanced, {b.size}, "|u1"); })
        .def_property_readonly("faults", [](TrainingBatch& b) { return array(b, b.faults, {b.size}, "<u4"); })
        .def_property_readonly("seed_indices", [](TrainingBatch& b) { return array(b, b.seed_indices, {b.size}, "<i8"); })
        .def_property_readonly("decision_counts", [](TrainingBatch& b) { return array(b, b.decision_counts, {b.size}, "<i8"); });
}
