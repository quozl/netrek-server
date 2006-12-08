#ifdef STURGEON
int sturgeon_hook_coup();
int sturgeon_hook_set_speed(int speed);
void sturgeon_apply_upgrade(int type, struct player *j, int multiplier);
void sturgeon_unapply_upgrade(int type, struct player *j, int multiplier);
#endif
