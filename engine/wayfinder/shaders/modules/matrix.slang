module matrix;

// Matrix inverses: Slang does not expose `inverse()` for these sizes for Wayfinder's SPIR-V path.
// Adapted from compute-toys/shadertoy.slang (MIT-style helper math). Same operation counts for 3×3
// as a straightforward cofactor expansion; 4×4 uses a block-style expansion that avoids the
// full naive 4×4 cofactor blow-up.

/// @brief Full inverse of a 2x2 matrix (general purpose).
public float2x2 MatrixInverse(float2x2 m)
{
    float det = m._11 * m._22 - m._12 * m._21;
    if (abs(det) < 1e-6)
        return float2x2(1, 0, 0, 1);
    float invD = 1.0 / det;
    return float2x2(m._22, -m._12, -m._21, m._11) * invD;
}

/// @brief Full inverse of a 3x3 matrix (general purpose).
public float3x3 MatrixInverse(float3x3 m)
{
    float3x3 inv;
    inv._11 = (m._22 * m._33 - m._23 * m._32);
    inv._12 = -(m._12 * m._33 - m._13 * m._32);
    inv._13 = (m._12 * m._23 - m._13 * m._22);

    inv._21 = -(m._21 * m._33 - m._23 * m._31);
    inv._22 = (m._11 * m._33 - m._13 * m._31);
    inv._23 = -(m._11 * m._23 - m._13 * m._21);

    inv._31 = (m._21 * m._32 - m._22 * m._31);
    inv._32 = -(m._11 * m._32 - m._12 * m._31);
    inv._33 = (m._11 * m._22 - m._12 * m._21);

    float det = m._11 * inv._11 + m._12 * inv._21 + m._13 * inv._31;
    if (abs(det) < 1e-6)
        return float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
    return inv * (1.0 / det);
}

/// @brief Full inverse of a 4×4 matrix (general purpose).
public float4x4 MatrixInverse(float4x4 m)
{
    float a00 = m._11, a01 = m._12, a02 = m._13, a03 = m._14;
    float a10 = m._21, a11 = m._22, a12 = m._23, a13 = m._24;
    float a20 = m._31, a21 = m._32, a22 = m._33, a23 = m._34;
    float a30 = m._41, a31 = m._42, a32 = m._43, a33 = m._44;

    float b00 = a00 * a11 - a01 * a10;
    float b01 = a00 * a12 - a02 * a10;
    float b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11;
    float b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30;
    float b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31;
    float b10 = a21 * a33 - a23 * a31;
    float b11 = a22 * a33 - a23 * a32;

    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (abs(det) < 1e-6)
        return float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    float invD = 1.0 / det;

    float4x4 inv;
    inv._11 = (a11 * b11 - a12 * b10 + a13 * b09) * invD;
    inv._12 = (-a01 * b11 + a02 * b10 - a03 * b09) * invD;
    inv._13 = (a31 * b05 - a32 * b04 + a33 * b03) * invD;
    inv._14 = (-a21 * b05 + a22 * b04 - a23 * b03) * invD;

    inv._21 = (-a10 * b11 + a12 * b08 - a13 * b07) * invD;
    inv._22 = (a00 * b11 - a02 * b08 + a03 * b07) * invD;
    inv._23 = (-a30 * b05 + a32 * b02 - a33 * b01) * invD;
    inv._24 = (a20 * b05 - a22 * b02 + a23 * b01) * invD;

    inv._31 = (a10 * b10 - a11 * b08 + a13 * b06) * invD;
    inv._32 = (-a00 * b10 + a01 * b08 - a03 * b06) * invD;
    inv._33 = (a30 * b04 - a31 * b02 + a33 * b00) * invD;
    inv._34 = (-a20 * b04 + a21 * b02 - a23 * b00) * invD;

    inv._41 = (-a10 * b09 + a11 * b07 - a12 * b06) * invD;
    inv._42 = (a00 * b09 - a01 * b07 + a02 * b06) * invD;
    inv._43 = (-a30 * b03 + a31 * b01 - a32 * b00) * invD;
    inv._44 = (a20 * b03 - a21 * b01 + a22 * b00) * invD;

    return inv;
}
