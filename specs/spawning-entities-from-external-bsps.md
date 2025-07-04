
One way to have prefab elements in the FTEQW engine is to use separate BSP or
MAP files that contain their own lighting, geometry and entities. These are
called bmodels, or brush models. By default, FTEQW does not handle the spawning
of entities from these files, and will ignore them. But with the
`FTE_TERRAIN_MAP` extension, you can spawn them in QuakeC.

## Example Code

```c
// spawn all child entities inside brush model
// NOTE: this must be called as self! it uses self.modelindex to get the bmodel
void spawn_child_entities()
{
	// get number of child entities
	int num_entities = (int)terrain_edit(TEREDIT_ENT_COUNT);

	// start at 1 so we don't create another worldspawn
	for (int entity_id = 1i; entity_id < num_entities; entity_id++)
	{
		// spawn child entity
		entity ent = spawn();

		// get entity data
		string entdata = (string)terrain_edit(TEREDIT_ENT_GET, entity_id);
		entdata = strcat("{", entdata, "}");

		// parse it into the entity we spawned
		parseentitydata(ent, entdata);

		// fix up origin and angles
		setorigin(ent, ent.origin + self.origin);
		ent.angles += self.angles;

		// get spawn function
		var void() spawnfunc = externvalue(0, ent.classname);

		// run spawnfunc as self
		entity oself = self;
		self = ent;
		spawnfunc();
		self = oself;
	}
}
```
