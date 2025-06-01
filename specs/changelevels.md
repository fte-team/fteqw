
The FTEQW engine offers many improvements in changelevel functionality over the
original Quake engine from 1996. In the original engine, your only way to send
player data across a level transition was through the `parm` globals in the
QuakeC server code, which meant you had exactly 16 floating-point numbers to
use to store any data the players needed to remember across a level change.
Everything else about the player's state was dropped. This worked well enough
for Quake, but is severely limiting if you need to send anything that couldn't
fit into a `float`.

Not only does FTEQW offer 64 `parm` globals instead of 16 (for DarkPlaces
compatibility), it also has a `parm_string` global which can store a
theoretically unlimited amount of string or buffer data. In this document I'll
show you how I decided to use it for my projects.

When the player spawns in a level for the first time, the function
`SetNewParms()` is called in the SSQC progs. This is where you set the default
`parm` values for every player that spawns in a level. For example, let's say
that `parm1` is going to store the player's health value across a level change.
You would set the default health value like so:

```c
void() SetNewParms =
{
	parm1 = 100;
};
```

When the engine is sending the players through a level transition, it calls the
function `SetChangeParms()` for each player entity in sequence. For storing the
player's health, you would do it like so:

```c
void() SetChangeParms =
{
	parm1 = self.health;
};
```

But, as stated before, this can get rather limiting if the player has data that
doesn't fit into a floating point number, such as a dynamic inventory of items
and weapons or an stats tree or something. This is where `parm_string` comes
in. If you're using a modern version of FTEQW and the FTEQCC compiler, you also
have access to several JSON parsing builtins. We will be using these to make
the usage of `parm_string` easier.

If we wanted to store more complex data in `parm_string`, the default value for
it should be a valid empty JSON node, like so:

```c
void() SetNewParms =
{
	parm1 = 100;
	parm_string = "{}";
};
```

And when `SetChangeParms()` is called, you will write your extended data as
valid JSON entries in `parm_string`, like so:

```c
void() SetChangeParms =
{
	parm1 = self.health;
	parm_string = strcat(
		"{\"some_extended_field_string\":\"",
		self.some_extended_field_string,
		"\"}"
	);
};
```

Note we are using the `strcat()` builtin to construct our JSON string rather
than `sprintf()`, because `sprintf()` seems to have some difficulties with very
long strings.

The above code will store a per-player string value called
`some_extended_field_string` into `parm_string` for storage across the level
transition. Ideally you'd wanna make some helper functions to easily write and
format your JSON, but this is just a basic example that constructs it directly.

Now the interesting part comes when it's time to read the data back. You'd want
to do this in the function `PutClientInServer()` after setting up your
fundamental player class, like so:

```c
void() PutClientInServer =
{
	// setup fundamental player state
	self.takedamage = DAMAGE_YES;
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_WALK;
	self.fixangle = TRUE;
	setsize(self, [-16, -16, -36], [16, 16, 36]);
	self.flags |= FL_CLIENT;
	self.view_ofs = [0, 0, 28];
	self.classname = "player";

	// load health value from parm1
	self.health = parm1;

	// parse parm_string as json
	jsonnode root = json_parse(parm_string);
	if (!root)
		error("couldn't decode parm_string as JSON!");

	// check if we have the field
	// if so, copy it out as a string
	if (root["some_extended_field_string"])
		self.some_extended_field_string = root["some_extended_field_string"].s;

	// clean up
	json_free(root);
};
```

You don't have to use `parm_string` as JSON, though. You could just print the
values as space-separated tokens and use the `tokenize()` or
`tokenize_console()` builtins to read them back. Either way, it's a good method
for storing lots of custom data across level transitions. Enjoy!
