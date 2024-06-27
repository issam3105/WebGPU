
@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

    let scaleMatrix = mat4x4f(
        1.1, 0.0, 0.0, 0.0,
        0.0, 1.1, 0.0, 0.0,
        0.0, 0.0, 1.1, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

	out.position = u_scene.projection * u_scene.view * u_node.model * scaleMatrix * vec4f(in.position, 1.0);
	out.color = in.color;
	out.normal = in.normal;
	out.uv = in.uv;
	
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = u_material.colorFactor;
	return vec4f(color.rgb, 1.0);
}