var<private> gl_VertexIndex : i32;

var<private> outColor : vec4f;

var<private> gl_Position : vec4f;

fn main_1() {
  var pos : vec2f;
  if ((gl_VertexIndex == 0i)) {
    pos = vec2f(-1.0f);
    outColor = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
  } else {
    if ((gl_VertexIndex == 1i)) {
      pos = vec2f(1.0f, -1.0f);
      outColor = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    } else {
      if ((gl_VertexIndex == 2i)) {
        pos = vec2f(0.0f, 1.0f);
        outColor = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      }
    }
  }
  gl_Position = vec4f(pos.x, pos.y, 0.0f, 1.0f);
  return;
}

struct main_out {
  @location(0)
  outColor_1 : vec4f,
  @builtin(position)
  gl_Position : vec4f,
}

@vertex
fn main(@builtin(vertex_index) gl_VertexIndex_param : u32) -> main_out {
  gl_VertexIndex = bitcast<i32>(gl_VertexIndex_param);
  main_1();
  return main_out(outColor, gl_Position);
}
