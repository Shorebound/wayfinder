module scene_globals;

/// Per-frame scene globals, std140 (matches C++ SceneGlobalsUBO).
public struct SceneGlobals
{
    public float3 light_direction;
    public float  light_intensity;
    public float3 light_colour;
    public float  ambient;
};
