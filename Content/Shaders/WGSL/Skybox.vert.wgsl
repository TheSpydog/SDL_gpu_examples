@group(0) @binding(0) var<uniform> MatrixTransform: mat4x4<f32>;

struct VertexOutput {
    @location(0) TexCoord: vec3<f32>,
    @builtin(position) Position: vec4<f32>,
}

@vertex
fn main(
    @location(0) inTexCoord: vec3<f32>
) -> VertexOutput {
    var output: VertexOutput;
    output.TexCoord = inTexCoord;
    output.Position = MatrixTransform * vec4<f32>(inTexCoord, 1.0);
    return output;
}
