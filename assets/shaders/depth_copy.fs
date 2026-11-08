// Copies the scene's depth (window space, 0..1) into a float colour target, so a later pass can sample it while the
// depth texture itself is its depth attachment (the Transparent stage's soft particles).
uniform sampler2D source_depth;

layout (location = 0) out float out_depth;

void main()
{
	out_depth = texelFetch(source_depth, ivec2(gl_FragCoord.xy), 0).r;
}
