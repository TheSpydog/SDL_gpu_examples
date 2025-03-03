struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

struct UBO {
    matrix: mat4x4<f32>,
};

@group(1) @binding(0) var<uniform> ubo: UBO;

@vertex
fn main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.color = input.color;
    output.clip_position = ubo.matrix * vec4<f32>(input.position, 1.0);
    return output;
}
