
float movement_time;
float movement_type;
vector movement_angles;
vector movement_velocity;
vector movement_origin;
vector movement_quantities;
float movement_buttons;
float movement_gravity;
float movement_maxspeed;

void() RunPlayerPhysics =
{
	local vector vel;
	local float speed;

	makevectors(movement_angles);


	vel = 	v_forward*movement_quantities_x +
		v_right*movement_quantities_y +
		v_up*movement_quantities_z;
	vel = movement_velocity+vel*movement_time;
	speed = vlen(vel);
	if (speed > maxspeed)
		vel = vel * (maxspeed/speed);
};