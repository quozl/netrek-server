/* blog.c */
void blog_file(char *class, char *file);
void blog_printf(char *class, const char *fmt, ...);
void blog_pickup_game_full();
void blog_pickup_game_not_full();
void blog_pickup_queue_full();
void blog_pickup_queue_not_full();
void blog_game_over(struct status *was, struct status *is);
void blog_base_loss(struct player *j);
