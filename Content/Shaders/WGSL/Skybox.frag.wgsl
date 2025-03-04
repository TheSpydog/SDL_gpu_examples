@group(1) @binding(0) var SkyboxSampler: sampler;
@group(1) @binding(1) var SkyboxTexture: texture_cube<f32>;

@fragment
fn main(
    @location(0) TexCoord: vec3<f32>,
) -> @location(0) vec4<f32> {
    return textureSample(SkyboxTexture, SkyboxSampler, TexCoord);
}
