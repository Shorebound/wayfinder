module test_module;

/// A simple exported struct for testing module imports.
public struct TestData
{
    public float3 value;

    /// Scales the value by the given factor.
    [mutating]
    public void Scale(float factor)
    {
        value = value * factor;
    }
};
