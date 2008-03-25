struct planet *planet_find(char *name);
struct planet *planet_by_number(char *name);

struct planet *pl_pick_home(int p_team);
void pl_pick_home_offset(int p_team, int *x, int *y);

struct planet *pl_virgin();
int pl_virgin_size();
void pl_reset();
