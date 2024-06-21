
const PI = 3.14159265359;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = u_scene.projection * u_scene.view * u_node.model * vec4f(in.position, 1.0);
	out.color = in.color;
	out.normal = in.normal;
	out.uv = in.uv;
	
	out.worldPosition = u_node.model * vec4f(in.position, 1.0);
	
	return out;
}

fn DistributionGGX(N : vec3f, H : vec3f, roughness : f32) -> f32
{
    let a = roughness*roughness;
    let a2 = a*a;
    let NdotH = max(dot(N, H), 0.0);
    let NdotH2 = NdotH*NdotH;

    let nom   = a2;
    var denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

fn GeometrySchlickGGX(NdotV : f32, roughness : f32) -> f32
{
    let r = (roughness + 1.0);
    let k = (r*r) / 8.0;

    let nom   = NdotV;
    let denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

fn GeometrySmith(N : vec3f, V : vec3f, L : vec3f, roughness: f32) ->f32
{
    let NdotV = max(dot(N, V), 0.0);
    let NdotL = max(dot(N, L), 0.0);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);
    let ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

fn fresnelSchlick(cosTheta : f32, F0 : vec3f) -> vec3f
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    //let lightDirection = vec3f(0.5, -0.9, 0.1);
	let lightDirection = u_scene.lightDirection.xyz;
	
    let V = normalize(u_scene.cameraPosition.xyz - in.worldPosition.xyz);
	let N = normalize(in.normal);
	//let L = normalize(lightPositions[i] - WorldPos);
	//let lightPosition = vec3f(0.5, -0.9, 0.1);
	//let L = normalize(lightDirection - in.worldPosition.xyz);
	let L = lightDirection;
	let H = normalize(V + L);
	
	let baseColor = textureSample(baseColorTexture, defaultSampler, in.uv) * u_uniforms.baseColorFactor;
	let metallicRoughnessTex = textureSample(metallicRoughnessTexture, defaultSampler, in.uv);
	let metallic = metallicRoughnessTex.b * u_uniforms.metallicFactor.x;
	let roughness = metallicRoughnessTex.g * u_uniforms.roughnessFactor.x;
	
	var  F0 = vec3f(0.04); 
    F0 = mix(F0, baseColor.xyz, metallic);
	
	let NDF = DistributionGGX(N, H, roughness); 
	let G   = GeometrySmith(N, V, L, roughness); 
	let F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
	
    let numerator    = NDF * G * F; 
    let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    let specular = numerator / denominator;
	
	// kS is equal to Fresnel
    let kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    var kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;	  

    // scale light by NdotL
    let NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    let Lo = (kD * baseColor.rgb / PI + specular) * NdotL; //* radiance
	
	
	// Gamma-correction
//	let srgb_color = pow(baseColor.rgb * shading, vec3f(2.2));
	//return vec4f(srgb_color, baseColor.a);
	return vec4f(Lo, 1.0);
}