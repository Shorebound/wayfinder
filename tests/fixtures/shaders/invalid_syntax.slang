// Deliberately broken shader for error-path testing.

struct VSOutput
{
    float4 Position : SV_Position
};  // missing semicolon on field above, extra semicolon here

[shader("vertex")]
VSOutput VSMain(UNDEFINED_TYPE input)  // undefined type
{
    VSOutput output;
    output.Position = UNDEFINED_FUNCTION();
    return output;
}
