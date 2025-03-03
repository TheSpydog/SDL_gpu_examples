var<private> FragColor : vec4f;

var<private> Color : vec4f;

fn main_1() {
  FragColor = Color;
  return;
}

struct main_out {
  @location(0)
  FragColor_1 : vec4f,
}

@fragment
fn main(@location(0) Color_param : vec4f) -> main_out {
  Color = Color_param;
  main_1();
  return main_out(FragColor);
}
