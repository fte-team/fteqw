//This file contains the system globals for the default pmove code

noref float input_timelength;
noref vector input_angles;
noref vector input_movevalues;
noref float input_buttons;
noref float input_impulse;
//float input_lightlevel;
//float input_weapon;
//float input_servertime;

float pmove_type;
vector pmove_org;
vector pmove_vel;
vector pmove_mins;
vector pmove_maxs;
float pmove_jump_held;
float pmove_waterjumptime;

float movevars_friction;
float movevars_gravity;

const float movevars_stepheight = 22;