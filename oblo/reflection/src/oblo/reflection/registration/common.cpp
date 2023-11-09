#include <oblo/reflection/registration/common.hpp>

#include <oblo/math/angle.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>

namespace oblo::reflection
{
    void register_fundamental_types(reflection_registry::registrant& reg)
    {
        reg.add_fundamental<bool>();

        reg.add_fundamental<char>();
        reg.add_fundamental<signed char>();
        reg.add_fundamental<unsigned char>();

        reg.add_fundamental<wchar_t>();

        reg.add_fundamental<char8_t>();
        reg.add_fundamental<char16_t>();
        reg.add_fundamental<char32_t>();

        reg.add_fundamental<signed short>();
        reg.add_fundamental<unsigned short>();

        reg.add_fundamental<signed int>();
        reg.add_fundamental<unsigned int>();

        reg.add_fundamental<signed long>();
        reg.add_fundamental<unsigned long>();

        reg.add_fundamental<signed long long>();
        reg.add_fundamental<unsigned long long>();

        reg.add_fundamental<float>();
        reg.add_fundamental<double>();
        reg.add_fundamental<long double>();
    }

    void register_math_types(reflection_registry::registrant& reg)
    {
        reg.add_class<radians>().add_field(&radians::value, "value");
        reg.add_class<degrees>().add_field(&degrees::value, "value");

        reg.add_class<vec2u>().add_field(&vec2u::x, "x").add_field(&vec2u::y, "y");

        reg.add_class<vec2>().add_field(&vec2::x, "x").add_field(&vec2::y, "y");

        reg.add_class<vec3>().add_field(&vec3::x, "x").add_field(&vec3::y, "y").add_field(&vec3::z, "z");

        reg.add_class<vec4>()
            .add_field(&vec4::x, "x")
            .add_field(&vec4::y, "y")
            .add_field(&vec4::z, "z")
            .add_field(&vec4::w, "w");

        reg.add_class<quaternion>()
            .add_field(&quaternion::x, "x")
            .add_field(&quaternion::y, "y")
            .add_field(&quaternion::z, "z")
            .add_field(&quaternion::w, "w");
    }
}