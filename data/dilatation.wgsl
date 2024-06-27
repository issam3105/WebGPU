
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
   // let color = textureSample(tex, defaultSampler, in.uv);
	var res = vec4f(0.0, 0.0, 0.0, 0.0);
	let SE_HALF_SIZE = 2.0; 
	
	for (var dy = -SE_HALF_SIZE; dy <= SE_HALF_SIZE; dy += 1.0) {
        let y = in.uv.y + dy * u_scene.viewTexel.y;
        for (var dx = -SE_HALF_SIZE; dx <= SE_HALF_SIZE; dx += 1.0) {
            let x = in.uv.x + dx * u_scene.viewTexel.x;
            let pix = textureSample(source, defaultSampler, vec2f(x, 1- y));
            res = max(res, pix);
        }
    }
	return res;
	//return  textureSample(source, defaultSampler, in.uv);
	// let color = textureSample(source, defaultSampler, in.uv);
	// return vec4f(res.rgb, 1.0);
}