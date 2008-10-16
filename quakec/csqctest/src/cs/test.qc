
void() runtest =
{
	self.frame1time = time;
};

void() test =
{
	precache_model("test/test.txt");
	self = spawn();
	setmodel(self, "test/test.txt");
	setorigin(self, player_local.origin);
	self.drawmask = 1;
	self.frame = floor(random()*2);
	self.predraw = runtest;
};