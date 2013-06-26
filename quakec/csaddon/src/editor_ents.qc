#define ENTS_NYI

#ifdef ENTS_NYI
void() editor_ents_add =
{
};

void(vector mousepos) editor_ents_overlay =
{
	drawrawstring('0 32 0', "Not Yet Implemented", '8 8 0', '1 1 1', 1);
};
#else
static float selectedent;
static entity tempent;

typedef struct
{
	string name;
	string value;
} entfield_t;

typedef struct
{
	//quick access for rendering.
	float modelindex;
	float alpha;
	vector org;
	float scale;

	entfield_t *fields;
	float numfields;
} entedit_t;

static entedit_t *editents;
static float numents;

float(entedit_t *ent, string field) editor_ents_findfield =
{
	float i;
	for (i = 0; i < ent->numfields; i++)
	{
		if (ent->fields[i].name == field)
			return i;
	}
	return -1;
};

void(entedit_t *ent, string field, string value) editor_ents_setfieldvalue =
{
	float f = editor_ents_findfield(ent, field);
	if (f < 0)
	{
		local entfield_t *nf;

		f = ent->numfields;
		ent->numfields = ent->numfields + 1;

		//extend the list
		nf = memalloc(sizeof(entfield_t)*ent->numfields);
		memcpy((__variant*)nf, (__variant*)ent->fields, sizeof(entfield_t)*f);
		memfree((__variant*)ent->fields);
		ent->fields = nf;
		ent->fields[f].name = strzone(field);
	}
	print(sprintf("set %s to %s\n", field, value));
	ent->fields[f].value = strzone(value);
};

entedit_t*() editor_ents_new =
{
	local float nent;
	local entedit_t *newents;
	nent = numents;
	numents += 1;

	//extend the list
	newents = memalloc(sizeof(entedit_t)*numents);
	memcpy((__variant*)newents, (__variant*)editents, sizeof(entedit_t)*nent);
	memfree((__variant*)editents);
	editents = newents;

print("new entity\n");
	return &editents[nent];
};

void(float num) editor_ents_delete =
{
};

void() editor_ents_reload =
{
	local string field, value;
print("RELOADING ENTS\n");

	//reset ent state
	getentitytoken(__NULL__);

	while (1)
	{
		field = getentitytoken();
		if not (field)
		{
			break;
		}
		if (field == "{")
		{
			local entedit_t *nent;
			nent = editor_ents_new();

			while(1)
			{
				field = getentitytoken();
				if not (field)
				{
					print("Truncated ent lump\n");
					return;
				}
				if (field == "}")
					break;
				value = getentitytoken();

				editor_ents_setfieldvalue(nent, field, value);
			}
		}
		else
		{
			print("Corrupt ent lump\n");
			return;
		}
	}
};

void() editor_ents_add =
{
	float e;
	if (!tempent)
	{
		tempent = spawn();
		editor_ents_reload();
	}

	for (e = 0; e < numents; e++)
	{
		//skip worldspawn...
		if (editents[e].modelindex == 1)
			continue;

		tempent.modelindex = editents[e].modelindex;
		tempent.alpha = editents[e].alpha;
		tempent.scale = editents[e].scale;

		if (e == selectedent)
		{
			if (gettime(0)*5f & 1)
				continue;
			tempent.effects |= 8192f;
		}
		else
			tempent.effects &~= 8192f;
		setorigin(tempent, editents[e].org);
		addentity(tempent);
	}
};

void(vector mousepos) editor_ents_overlay =
{
};
#endif
