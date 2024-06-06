@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = u_uniforms.projection * u_uniforms.view * u_uniforms.model * vec4f(in.position, 1.0);
	out.color = in.color;
	out.normal = in.normal;
	out.uv = in.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let lightDirection = vec3f(0.5, -0.9, 0.1);
	let lightColor = vec3f(1.0, 0.9, 0.6);
    let shading = dot(lightDirection, in.normal) * lightColor;
   // let color = in.color * u_uniforms.color.rgb * shading;
	let color = textureSample(baseColorTexture, defaultSampler, in.uv).rgb;
	// Gamma-correction
	let corrected_color = pow(color, vec3f(2.2));
	return vec4f(corrected_color, 1.0);
//	return vec4f(in.uv.x, in.uv.y, 0.0, 1.0);
}