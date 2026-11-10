float height_at(ivec2 texel)
{
	return texelFetch(texture_heights, texel, 0).r;
}

void main()
{
	ivec2 last_sample = ivec2(sample_count) - ivec2(1);
	// the last chunks of a terrain can be smaller than the grid: the vertices past the edge collapse onto it
	ivec2 texel = min(ivec2(chunk_origin) + ivec2(in_grid_position), last_sample);
	vec3 local_position = vec3(float(texel.x) * cell_size, height_at(texel), float(texel.y) * cell_size);

	// the slope from the neighbouring samples (one sided on the terrain's edges)
	ivec2 left = ivec2(max(texel.x - 1, 0), texel.y);
	ivec2 right = ivec2(min(texel.x + 1, last_sample.x), texel.y);
	ivec2 back = ivec2(texel.x, max(texel.y - 1, 0));
	ivec2 front = ivec2(texel.x, min(texel.y + 1, last_sample.y));
	float slope_x = (height_at(right) - height_at(left)) / (float(right.x - left.x) * cell_size);
	float slope_z = (height_at(front) - height_at(back)) / (float(front.y - back.y) * cell_size);
	vec3 local_normal = normalize(vec3(-slope_x, 1.0f, -slope_z));

	vec4 world_position = model * vec4(local_position, 1.0f);
	gl_Position = projection * view * world_position;
	vs_out.uv = local_position.xz / texture_tile_size;
	vs_out.normals = mat3(transpose(inverse(model))) * local_normal;
	vs_out.world_pos = world_position.xyz;
}
