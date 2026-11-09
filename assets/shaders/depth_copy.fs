// Scene Depth Copy: the scene's depth into a colour texture the Transparent pass can sample while SceneDepth is its
// depth attachment (soft particles).
uniform sampler2D texture_depth;

layout (location = 0) out float out_depth;

void main()
{
	out_depth = texelFetch(texture_depth, ivec2(gl_FragCoord.xy), 0).r;
}
