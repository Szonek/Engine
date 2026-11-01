// the owner of a debug line (a gizmo's game object) into the entity ids the editor picks from; lines without one leave
// what is behind them
flat in uint line_owner;
layout (location = 0) out uint out_entity_id;
void main()
{
	if (line_owner == 0u)
	{
		discard;
	}
	out_entity_id = line_owner;
}
