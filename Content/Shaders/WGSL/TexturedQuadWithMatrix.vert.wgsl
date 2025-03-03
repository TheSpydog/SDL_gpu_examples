struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) tex_coord: vec2<f32>
};

struct VertexOutput {
    @location(0) tex_coord: vec2<f32>,
    @builtin(position) position: vec4<f32>
};

struct UniformBlock {
    matrix_transform: mat4x4<f32>
};
@group(0) @binding(0) var<uniform> uniforms: UniformBlock;

@vertex
fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.tex_coord = in.tex_coord;
    out.position = uniforms.matrix_transform * vec4<f32>(in.position, 1.0);
    return out;
}
