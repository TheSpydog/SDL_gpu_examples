struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

@vertex
fn main(
    input: VertexInput,
    @builtin(instance_index) instance_index: u32
) -> VertexOutput {
    var output: VertexOutput;
    output.color = input.color;
    
    var pos = (input.position * 0.25) - vec3<f32>(0.75, 0.75, 0.0);
    pos.x += (f32(instance_index) % 4.0) * 0.5;
    pos.y += floor(f32(instance_index) / 4.0) * 0.5;
    
    output.clip_position = vec4<f32>(pos, 1.0);
    return output;
}
