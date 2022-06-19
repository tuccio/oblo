#version 450

#define MAX_BATCHES_COUNT 32
#define BINDING_INDEX(Major, Minor) Major* MAX_BATCHES_COUNT + Minor

#define REPEAT(N, Macro) REPEAT_##N(Macro)
#define REPEAT_0(Macro)
#define REPEAT_1(Macro) Macro(0) REPEAT_0(Macro)
#define REPEAT_2(Macro) Macro(1) REPEAT_1(Macro)
#define REPEAT_3(Macro) Macro(2) REPEAT_2(Macro)
#define REPEAT_4(Macro) Macro(3) REPEAT_3(Macro)
#define REPEAT_5(Macro) Macro(4) REPEAT_4(Macro)
#define REPEAT_6(Macro) Macro(5) REPEAT_5(Macro)
#define REPEAT_7(Macro) Macro(6) REPEAT_6(Macro)
#define REPEAT_8(Macro) Macro(7) REPEAT_7(Macro)
#define REPEAT_9(Macro) Macro(8) REPEAT_8(Macro)
#define REPEAT_10(Macro) Macro(9) REPEAT_9(Macro)
#define REPEAT_11(Macro) Macro(10) REPEAT_10(Macro)
#define REPEAT_12(Macro) Macro(11) REPEAT_11(Macro)
#define REPEAT_13(Macro) Macro(12) REPEAT_12(Macro)
#define REPEAT_14(Macro) Macro(13) REPEAT_13(Macro)
#define REPEAT_15(Macro) Macro(14) REPEAT_14(Macro)
#define REPEAT_16(Macro) Macro(15) REPEAT_15(Macro)
#define REPEAT_17(Macro) Macro(16) REPEAT_16(Macro)
#define REPEAT_18(Macro) Macro(17) REPEAT_17(Macro)
#define REPEAT_19(Macro) Macro(18) REPEAT_18(Macro)
#define REPEAT_20(Macro) Macro(19) REPEAT_19(Macro)
#define REPEAT_21(Macro) Macro(20) REPEAT_20(Macro)
#define REPEAT_22(Macro) Macro(21) REPEAT_21(Macro)
#define REPEAT_23(Macro) Macro(22) REPEAT_22(Macro)
#define REPEAT_24(Macro) Macro(23) REPEAT_23(Macro)
#define REPEAT_25(Macro) Macro(24) REPEAT_24(Macro)
#define REPEAT_26(Macro) Macro(25) REPEAT_25(Macro)
#define REPEAT_27(Macro) Macro(26) REPEAT_26(Macro)
#define REPEAT_28(Macro) Macro(27) REPEAT_27(Macro)
#define REPEAT_29(Macro) Macro(28) REPEAT_28(Macro)
#define REPEAT_30(Macro) Macro(29) REPEAT_29(Macro)
#define REPEAT_31(Macro) Macro(30) REPEAT_30(Macro)
#define REPEAT_32(Macro) Macro(31) REPEAT_31(Macro)

struct float3
{
    float x, y, z;
};

vec3 to_vec3(in const float3 f)
{
    return vec3(f.x, f.y, f.z);
}

#define DECLARE_IN_POSITION(Index) float3 in_Position##Index[];
#define DECLARE_IN_COLOR(Index) float3 in_Color##Index[];

#define DECLARE_POSITION_BUFFER(Index)                                                                                 \
    layout(std430, binding = BINDING_INDEX(0, Index)) buffer b_Position##Index                                         \
    {                                                                                                                  \
        float3 in_Position##Index[];                                                                                   \
    };
#define DECLARE_COLOR_BUFFER(Index)                                                                                    \
    layout(std430, binding = BINDING_INDEX(1, Index)) buffer b_Color##Index                                            \
    {                                                                                                                  \
        float3 in_Color##Index[];                                                                                      \
    };

REPEAT(32, DECLARE_POSITION_BUFFER)
REPEAT(32, DECLARE_COLOR_BUFFER)

layout(std430, binding = BINDING_INDEX(2, 0)) buffer b_BatchIndex
{
    uint in_BatchIndex[];
};

layout(location = 0) out vec3 out_Color;

layout(push_constant) uniform u_Constants
{
    vec3 translation;
    float scale;
}
c_Constants;

struct attributes
{
    vec3 position;
    vec3 color;
};

attributes read_attributes()
{
    const uint batchIndex = in_BatchIndex[gl_InstanceIndex];

    attributes result;

#define CASE(Index)                                                                                                    \
    case Index:                                                                                                        \
        result.position = to_vec3(in_Position##Index[gl_VertexIndex]);                                                 \
        result.color = to_vec3(in_Color##Index[gl_VertexIndex]);                                                       \
        break;

    // Hopefully this becomes a jump table
    switch (batchIndex)
    {
        REPEAT(32, CASE)
    }

#undef CASE

    return result;
}

void main()
{
    const attributes inAttributes = read_attributes();
    // gl_Position = vec4(inAttributes.position, 1.0);
    gl_Position = vec4(inAttributes.position * c_Constants.scale + c_Constants.translation, 1.0);
    out_Color = inAttributes.color;
    // out_Color = vec3(1, 0, 0);//inAttributes.color;
}