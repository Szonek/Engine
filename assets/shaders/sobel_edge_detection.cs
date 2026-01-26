#define SIMD_SIZE 32

layout(rgba8ui, binding = 0) uniform readonly uimage2D img_in;
// ToDo: we can write 1 byte to the output, consider changing it for performance boosts (?)
layout(rgba8ui, binding = 1) uniform uimage2D img_out;


layout (local_size_x = SIMD_SIZE, local_size_y = 4, local_size_z = 1) in;
void main() 
{
	const int weights_gx[9] = int[9]
	(
		-1, 0, 1,
		-2, 0, 2,
		-1, 0, 1
    );
	
	// Todo: this is just rotated weights_gx, ToDo: can be optimized by using index into weights_gx
	const int weights_gy[9] = int[9]
	(
		1, 2, 1,
		0, 0, 0,
		-1, -2, -1
    );
	
	ivec2 coord_base = ivec2(gl_GlobalInvocationID.xy);
	
	uvec4 value_gx = uvec4(0.0, 0.0, 0.0, 1.0);
	uvec4 value_gy = uvec4(0.0, 0.0, 0.0, 1.0);
	
	for(int k = 0; k < 9; k++)
	{
		int kh = -(k/3);
		int kw = -(k%3);
		
		ivec2 coord_in = ivec2(coord_base + ivec2(kh, kw));	
		uvec4 pixel = imageLoad(img_in, coord_in);
		
		// ToDo: we probably only care about single channel anyway.
		value_gx += (pixel * weights_gx[k]);
		value_gy += (pixel * weights_gy[k]);		
	}
	// ToDo: value could approxiamiation of magnitued of both convolutions abs(g_x) + abs(g_y)
	uvec4 value = (value_gx * value_gx) + (value_gy * value_gy);
	value = uvec4(sqrt(value));
	
	uvec4 in_pixel = imageLoad(img_out, coord_base);
	//ToDo: use mix?
	uvec4 final_pixel = in_pixel + value;
    imageStore(img_out, coord_base, final_pixel);
}