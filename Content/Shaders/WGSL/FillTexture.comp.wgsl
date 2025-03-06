@group(1) @binding(0) var outImage: texture_storage_2d<rgba8unorm, read_write>;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let coord: vec2<i32> = vec2<i32>(global_id.xy);
    textureStore(outImage, coord, vec4<f32>(1.0, 1.0, 0.0, 1.0));
}
