/*cppimport
<%
cfg['compiler_args'] = ['-std=c++14', '-fvisibility=hidden']
cfg['linker_args'] = ['-fvisibility=hidden']
cfg['include_dirs'] = ['sdsl-lite/include']
cfg['libraries'] = ['sdsl', 'divsufsort', 'divsufsort64']
cfg['dependencies'] = ['converters.hpp']
%>
*/

#include <algorithm>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include <sdsl/vectors.hpp>
#include <sdsl/enc_vector.hpp>
#include <sdsl/vlc_vector.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "converters.hpp"


namespace py = pybind11;


using sdsl::enc_vector;
using sdsl::int_vector;
using sdsl::vlc_vector;


template <class T>
auto add_std_algo(py::class_<T>& cls)
{
    cls.def(
        "max",
        [](const T &self) -> typename T::const_reference {
            return *std::max_element(self.begin(), self.end());
        },
        py::call_guard<py::gil_scoped_release>()
    );
    cls.def(
        "min",
        [](const T &self) {
            return *std::min_element(self.begin(), self.end());
        },
        py::call_guard<py::gil_scoped_release>()
    );
    cls.def(
        "minmax",
        [](const T &self) {
            auto result = std::minmax_element(self.begin(), self.end());
            return std::make_pair(*std::get<0>(result), *std::get<1>(result));
        },
        py::call_guard<py::gil_scoped_release>()
    );

    return cls;
}


template <class T, typename S = uint64_t>
auto add_class_(py::module &m, const char *name, const char *doc = nullptr)
{
    auto cls = py::class_<T>(m, name)
        .def(py::init([](const py::sequence& v) {
            const auto vsize = v.size();
            T result(vsize);
            for (size_t i = 0; i < vsize; i++)
            {
                result[i] = py::cast<typename T::value_type>(v[i]);
            }
            return result;
        }))
        .def_property_readonly("width", (uint8_t(T::*)(void) const) & T::width)
        .def_property_readonly("data",
                               (const uint64_t *(T::*)(void)const) & T::data)

        .def("__len__", &T::size, "The number of elements in the int_vector.")
        .def_property_readonly("size", &T::size,
                               "The number of elements in the int_vector.")
        .def_property_readonly_static(
            "max_size",
            [](py::object /* self */) { return T::max_size(); },
            "Maximum size of the int_vector."
        )
        .def_property_readonly(
            "size_in_mega_bytes",
            [](const T &self) { return sdsl::size_in_mega_bytes(self); }
        )
        .def_property_readonly("bit_size", &T::bit_size,
                               "The number of bits in the int_vector.")

        .def("resize", &T::resize,
             "Resize the int_vector in terms of elements.")
        .def("bit_resize", &T::bit_resize,
             "Resize the int_vector in terms of bits.")
        .def_property_readonly("capacity", &T::capacity,
                               "Returns the size of the occupied bits of the "
                               "int_vector. The capacity of a int_vector is "
                               "greater or equal to the bit_size of the "
                               "vector: capacity >= bit_size).")

        .def(
            "__getitem__",
            [](const T &self, size_t position) -> S {
            if (position >= self.size())
            {
                throw py::index_error(std::to_string(position));
            }
            return self[position];
            }
        )
        .def(
            "__setitem__",
            [](T &self, size_t position, S value) {
                if (position >= self.size())
                {
                    throw py::index_error(std::to_string(position));
                }
                self[position] = value;
            }
        )

        .def("set_to_id",
             [](T &self) { sdsl::util::set_to_id(self); },
             py::call_guard<py::gil_scoped_release>(),
             "Sets each entry of the vector at position `i` to value `i`")
        .def("set_to_value",
             [](T &self, S value) { sdsl::util::set_to_value(self, value); },
             py::call_guard<py::gil_scoped_release>(),
             py::arg("k"),
             "Set all entries of int_vector to value k. This method "
             "pre-calculates the content of at most 64 words and then "
             "repeatedly inserts these words."
        )
        .def("set_zero_bits",
             [](T &self) { sdsl::util::_set_zero_bits(self); },
             py::call_guard<py::gil_scoped_release>(),
             "Sets all bits of the int_vector to 0-bits.")
        .def("set_one_bits",
             [](T &self) { sdsl::util::_set_one_bits(self); },
             py::call_guard<py::gil_scoped_release>(),
             "Sets all bits of the int_vector to 1-bits.")
        .def(
            "set_random_bits",
            [](T &self, int seed) {
                sdsl::util::set_random_bits(self, seed);
            },
            py::call_guard<py::gil_scoped_release>(),
            py::arg_v(
                "seed",
                0,
                "If seed = 0, the time is used to initialize the pseudo "
                "random number generator, otherwise the seed parameter is used."
            ),
            "Sets all bits of the int_vector to pseudo-random bits."
        )
        .def(
            "__imod__",
            [](T &self, uint64_t m) {
                sdsl::util::mod(self, m);
                return self;
            },
            py::is_operator()
        )

        .def("cnt_one_bits",
            [](const T &self) { return sdsl::util::cnt_one_bits(self); },
            "Number of set bits in vector")
        .def("cnt_onezero_bits",
             [](const T &self) { return sdsl::util::cnt_onezero_bits(self); },
             "Number of occurrences of bit pattern `10` in vector")
        .def("cnt_zeroone_bits",
             [](const T &self) { return sdsl::util::cnt_zeroone_bits(self); },
             "Number of occurrences of bit pattern `01` in vector")

        .def(
            "next_bit",
            [](const T &self, size_t idx) {
                if (idx >= self.bit_size())
                {
                    throw py::index_error(std::to_string(idx));
                }
                return sdsl::util::next_bit(self, idx);
            },
            py::arg("idx"),
            "Get the smallest position `i` >= `idx` where a bit is set"
        )
        .def(
            "prev_bit",
            [](const T &self, size_t idx) {
                if (idx >= self.bit_size())
                {
                    throw py::index_error(std::to_string(idx));
                }
                return sdsl::util::prev_bit(self, idx);
            },
            py::arg("idx"),
            "Get the largest position `i` <= `idx` where a bit is set"
        )

        .def("__str__",
             [](const T &self) { return sdsl::util::to_string(self); })
        .def("to_latex",
             [](const T &self) { return sdsl::util::to_latex_string(self); })
    ;

    add_std_algo(cls);

    if (doc) cls.doc() = doc;

    return cls;
}


template <class T, class Tup>
auto add_enc_class(py::module &m, const std::string& name, Tup init_from,
                   const char* doc = nullptr)
{
    auto cls = py::class_<T>(m, name.c_str())
        .def(py::init())

        .def("__len__", &T::size, "The number of elements in the vector.")
        .def_property_readonly("size", &T::size,
                               "The number of elements in the vector.")
        .def_property_readonly_static(
            "max_size",
            [](py::object /* self */) {
                return T::max_size();
            },
            "The largest size that this container can ever have."
        )
        .def_property_readonly(
            "size_in_mega_bytes",
            [](const T &self) { return sdsl::size_in_mega_bytes(self); }
        )

        .def(
            "__getitem__",
            [](const T &self, size_t position) {
                if (position >= self.size())
                {
                    throw py::index_error(std::to_string(position));
                }
                return self[position];
            }
        )

        //.def("get_sample_dens", &T::get_sample_dens)

        .def("__str__",
             [](const T &self) { return sdsl::util::to_string(self); })
        .def("to_latex",
             [](const T &self) { return sdsl::util::to_latex_string(self); })
    ;

    for_each_in_tuple(init_from, make_inits_functor(cls));

    cls.def(py::init(
        [](const std::vector<uint64_t>& source) {
            py::print("Slow"); return T(source);
        }
    ));

    // if (sample)
    // {
    //     cls.def(
    //         "sample",
    //         [](const T &self, typename T::size_type i) {
    //             if (i >= self.size() / self.get_sample_dens())
    //             {
    //                 throw py::index_error(std::to_string(i));
    //             }
    //             return self.sample(i);
    //         },
    //          "Returns the i-th sample of the compressed vector"
    //          "i: The index of the sample. 0 <= i < size()/get_sample_dens()"
    //     );
    // }

    add_std_algo(cls);

    if (doc) cls.doc() = doc;

    return cls;
}

template <class VTuple>
class add_enc_coders_functor
{
public:
    add_enc_coders_functor(py::module& m, const VTuple& iv_classes):
    m(m), iv_classes(iv_classes) {}

    template <typename Coder>
    void operator()(const std::pair<const char*, Coder> &t)
    {
        add_enc_class<enc_vector<Coder>>(
            m,
            std::string("EncVector") + std::get<0>(t),
            iv_classes,
            "A vector `v` is stored more space-efficiently by "
            "self-delimiting coding the deltas v[i+1]-v[i] (v[-1]:=0)."
        );
    }

private:
    py::module& m;
    const VTuple& iv_classes;
};

template <class VTuple>
auto make_enc_coders_functor(py::module& m, const VTuple& iv_classes)
{
    return add_enc_coders_functor<VTuple>(m, iv_classes);
}


template <class VTuple>
class add_vlc_coders_functor
{
public:
    add_vlc_coders_functor(py::module& m, const VTuple& iv_classes):
    m(m), iv_classes(iv_classes) {}

    template <typename Coder>
    void operator()(const std::pair<const char*, Coder> &t)
    {
        add_enc_class<vlc_vector<Coder>>(
            m,
            std::string("VlcVector") + std::get<0>(t),
            iv_classes,
            "A vector which stores the values with variable length codes."
        );
    }

private:
    py::module& m;
    const VTuple& iv_classes;
};

template <class VTuple>
auto make_vlc_coders_functor(py::module& m, const VTuple& iv_classes)
{
    return add_vlc_coders_functor<VTuple>(m, iv_classes);
}


PYBIND11_MODULE(pysdsl, m)
{
    m.doc() = "sdsl-lite bindings for python";

    auto iv_classes = std::make_tuple(
        add_class_<int_vector<0>>(m, "IntVector",
                                  "This generic vector class could be used to "
                                  "generate a vector that contains integers "
                                  "of fixed width `w` in [1..64].")
            .def(
                py::init([](size_t size,
                            uint64_t default_value,
                            uint8_t bit_width) {
                    return int_vector<0>(size, default_value, bit_width);
                }),
                py::arg("size") = 0,
                py::arg("default_value") = 0,
                py::arg("bit_width") = 64)
            .def(
                "expand_width",
                [](int_vector<0> &self, size_t width) {
                    sdsl::util::expand_width(self, width);
                },
                "Expands the integer width to new_width >= v.width()."
            )
            .def("bit_compress",
                [](int_vector<0> &self) { sdsl::util::bit_compress(self); },
                "Bit compress the int_vector. Determine the biggest value X "
                "and then set the int_width to the smallest possible so that "
                "we still can represent X."),

        add_class_<int_vector<1>, bool>(m, "BitVector")
            .def(py::init([](size_t size, bool default_value) {
                    return int_vector<1>(size, default_value, 1);
                }), py::arg("size") = 0, py::arg("default_value") = false)
            .def("flip", &int_vector<1>::flip, "Flip all bits of bit_vector"),

        add_class_<int_vector<8>, uint8_t>(m, "Int8Vector")
            .def(py::init([](size_t size, uint8_t default_value) {
                    return int_vector<8>(size, default_value, 8);
                }), py::arg("size") = 0, py::arg("default_value") = 0),

        add_class_<int_vector<16>, uint16_t>(m, "Int16Vector")
            .def(py::init([](size_t size, uint16_t default_value) {
                     return int_vector<16>(size, default_value, 16);
                 }), py::arg("size") = 0, py::arg("default_value") = 0),

        add_class_<int_vector<32>, uint32_t>(m, "Int32Vector")
            .def(py::init([](size_t size, uint32_t default_value) {
                    return int_vector<32>(size, default_value, 32);
                }), py::arg("size") = 0, py::arg("default_value") = 0),

        add_class_<int_vector<64>, uint64_t>(m, "Int64Vector")
            .def(py::init([](size_t size, uint64_t default_value) {
                    return int_vector<64>(size, default_value, 64);
                }), py::arg("size") = 0, py::arg("default_value") = 0)
    );

    auto coders = std::make_tuple(
        std::make_pair("EliasDelta", sdsl::coder::elias_delta()),
        std::make_pair("EliasGamma", sdsl::coder::elias_gamma()),
        std::make_pair("Fibonacci", sdsl::coder::fibonacci()),
        std::make_pair("Comma2", sdsl::coder::comma<2>()),
        std::make_pair("Comma4", sdsl::coder::comma<4>())
    );

    for_each_in_tuple(coders, make_enc_coders_functor(m, iv_classes));
    for_each_in_tuple(coders, make_vlc_coders_functor(m, iv_classes));

}
