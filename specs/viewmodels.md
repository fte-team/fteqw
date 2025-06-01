
In FTEQW, you can use CSQC to give you more control over weapon viewmodels.

Here is a chunk of code showing how to spawn a viewmodel entity in CSQC, with
the appropriate flags set.

```c
static entity viewmodel;

void CSQC_Init(float apilevel, string enginename, float engineversion)
{
	viewmodel = spawn();
	viewmodel.drawmask = MASK_VIEWMODEL;
	viewmodel.renderflags |= RF_VIEWMODEL;
	viewmodel.effects |= EF_NOSHADOW;
	// you may have to rotate the viewmodel angles depending on how you
	// exported it from your modeling software
	viewmodel.angles = [0, 0, 0];
	setorigin(viewmodel, [0, 0, 0]);
	setsize(viewmodel, [0, 0, 0], [0, 0, 0]);
}
```

...and if your model is skeletally animated, you would do this in
`CSQC_UpdateView`:

```c
void CSQC_UpdateView(float vwidth, float vheight, float notmenu)
{
	// push viewmodel animation at constant framerate
	// note the use of frametime rather than clframetime, you probably don't
	// want the viewmodel animating while the game is paused
	viewmodel.frame1time += frametime;

	// other stuff down here...
}
```

Note that we have not set a model on it yet, so it will not yet be visible. To
do that, we should first figure out what weapon the player is holding. There's
a few different ways you could do this, but I'm gonna show you the Quake-y way.
To get the modelindex of the player's viewmodel, you could do this:

```c
setmodelindex(viewmodel, getstatf(STAT_WEAPONMODELI));
```

`STAT_WEAPONMODELI` is a networked player stat that corresponds to the
modelindex of the model specified by `self.weaponmodel` in the SSQC module. You
should probably call the above code only if you detect that the player has
switched weapons, or if the viewmodel's current modelindex is different from
the value read from `STAT_WEAPONMODELI`. You can also read the player's current
weapon index by reading the stat `STAT_ACTIVEWEAPON`, though you must set this
yourself in the SSQC module with the `self.weapon` field, otherwise it will
mean nothing.

To set the weapon's animation, you could set `self.weaponframe` in SSQC and
then read it from CSQC with the `STAT_WEAPONFRAME` stat. In the CSQC module
you'd then do:

```c
viewmodel.frame = getstatf(STAT_WEAPONFRAME);
```

Note that whenever a new animation starts on the viewmodel, you should set
`viewmodel.frame1time` back to 0. This will reset the animation time.
