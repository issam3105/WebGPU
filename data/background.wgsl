
@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = vec4f(in.position, 1.0);
	out.tangent = in.tangent;
	out.normal = in.normal;
	out.uv = in.uv;
	
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = textureSample(backgroundTexture, defaultSampler, vec2f(in.uv.x, 1.0- in.uv.y)) * u_scene.backgroundFactor;
	//return vec4f(color.rgb, 1.0);
	return color;
	//return vec4f(vec3f(color.a), 1.0);
}