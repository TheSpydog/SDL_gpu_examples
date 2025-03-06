@group(1) @binding(0) var outImage: texture_storage_2d<bgra8unorm, read_write>;

struct UBO {
    ubo_time: f32,
}
@group(2) @binding(0) var<uniform> ubo: UBO;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let size: vec2<u32> = textureDimensions(outImage);
    let coord: vec2<i32> = vec2<i32>(global_id.xy);
    let uv: vec2<f32> = vec2<f32>(coord) / vec2<f32>(size);

    let col: vec3<f32> = vec3<f32>(0.5) + (cos((vec3<f32>(ubo.ubo_time) + uv.xyx) + vec3<f32>(0.0, 2.0, 4.0)) * 0.5);
    textureStore(outImage, coord, vec4<f32>(col, 1.0));
}
