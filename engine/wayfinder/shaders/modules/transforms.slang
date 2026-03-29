module transforms;

/// Vertex UBO for lit shaders (128 bytes: two float4x4, matches C++ TransformUBO).
public struct TransformData
{
    public float4x4 mvp;
    public float4x4 model;
};

/// Vertex UBO for unlit shaders (64 bytes: one float4x4, matches C++ UnlitTransformUBO).
public struct UnlitTransformData
{
    public float4x4 mvp;
};
