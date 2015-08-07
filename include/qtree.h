
#ifndef QTREE_H
#define QTREE_H

#include <X11/Xlib.h>

#define BODYMAX 4096

typedef struct point_t point_t;
typedef struct rectangle_t rectangle_t;
typedef struct body_t body_t;
typedef struct qtree_t qtree_t;

/* coordinates of point */
struct point_t {
  double x;
  double y;
};

/* rectangle */
struct rectangle_t {
  point_t *vertex;
  double dx;
  double dy;
};

/* info of body */
struct body_t {
  point_t *pos; 
  double mass;
};

/* quad tree */
struct qtree_t {
  qtree_t *ur; /* upper right */
  qtree_t *ul; /* upper left */
  qtree_t *ll; /* lower left */
  qtree_t *lr; /* lower right */
  rectangle_t *range;
  body_t *body[BODYMAX];
  int count; /* number of bodies in the range*/
};

point_t *point_create(double x, double y);
void point_free(point_t **p);
point_t *point_in_window(point_t *p, point_t *base, double ratio, 
                         double shift);
rectangle_t *rectangle_create(double x, double y, double dx, double dy); 
void rectangle_free(rectangle_t **r);
int is_point_in_rectangle(point_t *p, rectangle_t *r, int rectangle);
int is_point_in_rectangle_ur(point_t *p, rectangle_t *r);
int is_point_in_rectangle_ul(point_t *p, rectangle_t *r);
int is_point_in_rectangle_ll(point_t *p, rectangle_t *r);
int is_point_in_rectangle_lr(point_t *p, rectangle_t *r);
body_t *body_create(double x, double y, double mass);
void body_free(body_t **b);
rectangle_t *body_range(int count, body_t *(*body)[BODYMAX]);
qtree_t *qtree_add(double x, double y, double dx, double dy);
void qtree_remove(qtree_t **qtree);
int qtree_push_body(qtree_t *qtree, body_t *body);
int qtree_pop_body(qtree_t *qtree);
int qtree_clean_body(qtree_t *qtree);
int qtree_split(qtree_t *qtree);
void qtree_pickbody(qtree_t *qtree, int count, body_t *(*body)[BODYMAX],
                    int rectangle);
qtree_t *qtree_create(int count, body_t *(*body)[BODYMAX]);
void *qtree_construct(void *root_count_body);
void *qtree_destruct(void *root);
void wait_qtree(int body_num);
void qtree_traverse(qtree_t *root);
void qtree_traverse_draw_range(qtree_t *root, Display *dpy, Window w, GC gc,
                               point_t *base, double ratio, double shift);
void qtree_destroy(qtree_t **root);
#endif
