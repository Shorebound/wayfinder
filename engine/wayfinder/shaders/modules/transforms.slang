module transforms;

import "modules/matrix";

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

/// Upper 3×3 inverse-transpose of `model` for vertex normals (handles non-uniform scale).
public float3x3 NormalMatrixFromModel(float4x4 model)
{
    return transpose(MatrixInverse((float3x3)model));
}
