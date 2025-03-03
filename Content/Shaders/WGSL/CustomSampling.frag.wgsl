struct UBO {
    mode: i32,
};

@group(2) @binding(0)
var texture: texture_storage_2d<rgba8unorm, read>;

@group(3) @binding(0)
var<uniform> ubo: UBO;

@fragment
fn main(
    @location(0) TexCoord: vec2<f32>
) -> @location(0) vec4<f32> {
    let texture_size = textureDimensions(texture);
    let texel_pos = vec2<i32>(vec2<f32>(texture_size) * TexCoord);
    let main_texel = textureLoad(texture, texel_pos);

    if ubo.mode == 0 {
        return main_texel;
    } else {
        let bottom_texel = textureLoad(texture, texel_pos + vec2<i32>(0, 1));
        let left_texel = textureLoad(texture, texel_pos + vec2<i32>(-1, 0));
        let top_texel = textureLoad(texture, texel_pos + vec2<i32>(0, -1));
        let right_texel = textureLoad(texture, texel_pos + vec2<i32>(1, 0));
        return 0.2 * (main_texel + bottom_texel + left_texel + top_texel + right_texel);
    }
}
