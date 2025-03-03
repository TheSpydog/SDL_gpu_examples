@group(0) @binding(0) var in_texture: texture_2d<f32>;
@group(0) @binding(1) var in_sampler: sampler;

@group(0) @binding(2) var out_texture: texture_storage_2d<rgba8unorm, write>;

struct UBO {
    texcoord_multiplier: f32,
}
@group(0) @binding(3) var<uniform> ubo: UBO;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let coord = vec2<i32>(global_id.xy);
    let tex_size = textureDimensions(in_texture);
    let texcoord = (ubo.texcoord_multiplier * vec2<f32>(coord)) / vec2<f32>(tex_size);
    
    let in_pixel = textureSample(in_texture, in_sampler, texcoord);
    textureStore(out_texture, coord, in_pixel);
}
