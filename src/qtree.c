
#include <float.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "qtree.h"
#include "threadpool.h"


static int leaf_count = 0;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;

/**
 * @brief argument for qtree_pickbody run by thread 
 */
typedef struct qtree_construct_t qtree_construct_t;
enum {upperright = 1, upperleft = 2, lowerleft = 3, lowerright = 4}; 
struct qtree_construct_t {
  qtree_t *root;
  int rectangle;
  int count;
  body_t *(*body)[BODYMAX];
};

static qtree_construct_t*
qtree_construct_setup(qtree_t *root, int rectangle, 
                      int count, body_t *(*body)[BODYMAX]);

static qtree_construct_t*
qtree_construct_setup(qtree_t *root, int rectangle, 
                      int count, body_t *(*body)[BODYMAX]) {
  qtree_construct_t *arg;
  arg = malloc(sizeof(qtree_construct_t));
  if (!arg) {
    fprintf(stderr, "Error: fail to malloc.\n");
    return NULL;
  }
  arg->root = root;
  arg->rectangle = rectangle;
  arg->count = count;
  arg->body = body;
  return arg;
}


/**
 * @return pointer or NULL if fails
 */
point_t *point_create(double x, double y) {
  point_t *p = malloc(sizeof(point_t));
  if (!p) {
    fprintf(stderr, "Error: fails to malloc.\n");
    goto err;
  }
  p->x = x;
  p->y = y;
  return p;
err:
  return NULL;
}

void point_free(point_t **p) {
  free(*p);
  *p = NULL;
}

/**
 * @brief compute the site of the point on the window 
 */
point_t *point_in_window(point_t *p, point_t *base, double ratio, 
                         double shift) {
  double x, y;
  x = shift + (p->x - base->x) * ratio;
  y = shift + (p->y - base->y) * ratio;
  return point_create(x, y);
}

/**
 * @return pointer or NULL if fails
 */
rectangle_t *rectangle_create(double x, double y, double dx, double dy) {
  point_t *p;
  rectangle_t *r;
  
  p = point_create(x, y);
  if (!p) {
    fprintf(stderr, "Error: fail to malloc.\n");
    goto p_err;
  }
  r = malloc(sizeof(rectangle_t));
  if (!r) {
    fprintf(stderr, "Error: fails to malloc.\n");
    goto r_err;
  }
  r->vertex = p;
  r->dx = dx;
  r->dy = dy;
  return r;
r_err:
  point_free(&p);
p_err:
  return NULL;
}

void rectangle_free(rectangle_t **r) {
  point_free(&(*r)->vertex);
  free(*r);
  *r = NULL;
}

/**
 * @return 1 if in or 0 if not in or -1 if error
 */
int is_point_in_rectangle(point_t *p, rectangle_t *r, int rectangle) {
  if (rectangle == upperright) {
    return is_point_in_rectangle_ur(p, r);
  }
  if (rectangle == upperleft) {
    return is_point_in_rectangle_ul(p, r);
  }
  if (rectangle == lowerleft) {
    return is_point_in_rectangle_ll(p, r);
  }
  if (rectangle == lowerright) {
    return is_point_in_rectangle_lr(p, r);
  }
  return -1;
}

int is_point_in_rectangle_ur(point_t *p, rectangle_t *r) {
  if (r->vertex->x <= p->x && p->x <= r->vertex->x + r->dx &&
      r->vertex->y <= p->y && p->y <= r->vertex->y + r->dy) {
    return 1;
  }
  return 0;
}

int is_point_in_rectangle_ul(point_t *p, rectangle_t *r) {
  if (r->vertex->x <= p->x && p->x < r->vertex->x + r->dx &&
      r->vertex->y <= p->y && p->y <= r->vertex->y + r->dy) {
    return 1;
  }
  return 0;
}

int is_point_in_rectangle_ll(point_t *p, rectangle_t *r) {
  if (r->vertex->x <= p->x && p->x < r->vertex->x + r->dx &&
      r->vertex->y <= p->y && p->y < r->vertex->y + r->dy) {
    return 1;
  }
  return 0;
}

int is_point_in_rectangle_lr(point_t *p, rectangle_t *r) {
  if (r->vertex->x <= p->x && p->x <= r->vertex->x + r->dx &&
      r->vertex->y <= p->y && p->y < r->vertex->y + r->dy) {
    return 1;
  }
  return 0;
}

/**
 * @return pointer or NULL if fails
 */
body_t *body_create(double x, double y, double mass) {
  point_t *pos;
  body_t *b;

  pos = point_create(x, y);
  if (!pos) {
    fprintf(stderr, "Error: fail to malloc.\n");
    goto pos_err;
  }
  b = malloc(sizeof(body_t));
  if (!b) {
    fprintf(stderr, "Error: fail to malloc.\n");
    goto b_err;
  }
  pos->x = x;
  pos->y = y;
  b->pos = pos;
  b->mass = mass;
  return b;
b_err:
  point_free(&pos);
pos_err:
  return NULL;
}

void body_free(body_t **b) {
  point_free(&(*b)->pos);
  free(*b);
  *b = NULL;
}

rectangle_t *body_range(int count, body_t *(*body)[BODYMAX]) {
  int i;
  double max_x = DBL_MIN, max_y = DBL_MIN;
  double min_x = DBL_MAX, min_y = DBL_MAX;
  double x, y, dx, dy;
  rectangle_t *range;

  for (i = 0; i < count; i++) {
    x = (*body)[i]->pos->x;
    y = (*body)[i]->pos->y;
    min_x = (x < min_x) ? x : min_x;
    min_y = (y < min_y) ? y : min_y;
    max_x = (max_x < x) ? x : max_x;
    max_y = (max_y < y) ? y : max_y;
  }
  dx = max_x - min_x;
  dy = max_y - min_y;
  dx = dy = (dx < dy) ? (dy + 10) : (dx + 10);
  x = (max_x + min_x) / 2 - dx / 2;
  y = (max_y + min_y) / 2 - dy / 2;

  range = rectangle_create(x, y, dx, dy);
  if (!range) {
    fprintf(stderr, "Error: fail to create rectangle.\n");
    return NULL;
  }
  return range;
}

/**
 * @return pointer or NULL if fails
 */
qtree_t *qtree_add(double x, double y, double dx, double dy) {
  rectangle_t *r;
  qtree_t *q;
  r = rectangle_create(x, y, dx, dy);
  if (!r) {
    fprintf(stderr, "Error: fail to malloc.\n");
    goto r_err;
  }
  q = malloc(sizeof(qtree_t));
  if (!q) {
    fprintf(stderr, "Error: fail to malloc.\n");
    goto q_err;
  }
  q->ur = NULL;
  q->ul = NULL;
  q->ll = NULL;
  q->lr = NULL;
  q->range = r;
  q->count = 0;

  return q;
q_err:
  rectangle_free(&r);
r_err:
  return NULL;  
}

void qtree_remove(qtree_t **qtree) {
  rectangle_free(&(*qtree)->range);
  qtree_clean_body(*qtree);
  free(*qtree);
  (*qtree) = NULL;
}

/**
 * @return 0 if success or -1 if fail
 */
int qtree_push_body(qtree_t *qtree, body_t *body) {
  if (qtree->count == BODYMAX) {
    fprintf(stderr, "Error: body stack is full.\n");
    return -1;
  }
  qtree->body[qtree->count++] = body;
  return 0;  
}

/**
 * @return 0 if success or -1 if fail
 */
int qtree_pop_body(qtree_t *qtree) {
  if (qtree->count == 0) {
    fprintf(stderr, "Error: body stack is empty.\n");
    return -1;
  }
  qtree->body[--qtree->count];
  return 0;
}

/**
 * @return 0 if success or -1 if fail
 */
int qtree_clean_body(qtree_t *qtree) {
  while (qtree->count > 0) {
    if (qtree_pop_body(qtree)) {
      fprintf(stderr, "Error: fail to pop body.\n");
      return -1;
    }
  }
  return 0;
}

/**
 * @brief add 4 childs
 */
int qtree_split(qtree_t *qtree) {
  rectangle_t *r;
  point_t *v;
  double dx, dy;

  if (!qtree) {
    fprintf(stderr, "Error: qtree is NULL.\n");
    return -1;
  }
  r = qtree->range;
  v = r->vertex;
  dx = r->dx / 2;
  dy = r->dy / 2;
  qtree->ur = qtree_add(v->x + dx, v->y + dy, dx, dy);
  qtree->ul = qtree_add(v->x, v->y + dy, dx, dy);
  qtree->ll = qtree_add(v->x, v->y, dx, dy);
  qtree->lr = qtree_add(v->x + dx, v->y, dx, dy);
  if (!qtree->ur || !qtree->ul || !qtree->ll || !qtree->lr) {
    fprintf(stderr, "Error: fail to split qtree.\n");
    return -1;
  }
  return 0;
}

/**
 * @brief pick up bodies which is in the range
 */
void qtree_pickbody(qtree_t *qtree, int count, body_t *(*body)[BODYMAX], 
                    int rectangle) {
  /* initial */
  int i;
  /* pick up */
  for (i = 0; i < count; i++) {
    if (is_point_in_rectangle((*body)[i]->pos, qtree->range, rectangle)) {
      qtree->body[qtree->count++] = (*body)[i];
    }
  }
}

qtree_t *qtree_create(int count, body_t *(*body)[BODYMAX]) {
  rectangle_t *root_range;
  qtree_t *root;
  qtree_construct_t *arg_ur, *arg_ul, *arg_ll, *arg_lr;
  extern threadpool_t *threadpool;
  int i;

  root_range = body_range(count, body);
  if (!root_range) {
    fprintf(stderr, "Error: fail to create root range.\n");
    goto root_range_err;
  }

  root = qtree_add(root_range->vertex->x, root_range->vertex->y,
                   root_range->dx, root_range->dy);
  if (!root) {
    fprintf(stderr, "Error: fail to create root.\n");
    goto root_err;
  }

  for (i = 0; i < count; i++) {
    qtree_push_body(root, (*body)[i]);
  }
  
/*
  printf("========================\n"); 
  printf("(%lf, %lf) dx=%lf dy=%lf\n", root->range->vertex->x,
                                       root->range->vertex->y,
                                       root->range->dx,
                                       root->range->dy);
  printf("count=%d\n", root->count);
  printf("========================\n"); 
*/

  /* only has 0 or 1 body */
  if (root->count == 0) {
    return root;
  }
  if (root->count == 1) {
    pthread_mutex_lock(&mutex);
    leaf_count++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return root;
  }
  /* add 4 childs */
  if (qtree_split(root)) {
    fprintf(stderr, "Error: fail to split.\n");
    goto split_err;
  }
  /* set arguments for task run by thread */
  arg_ur = qtree_construct_setup(root->ur, upperright,
                                 root->count, &root->body);
  arg_ul = qtree_construct_setup(root->ul, upperleft, 
                                 root->count, &root->body);
  arg_ll = qtree_construct_setup(root->ll, lowerleft, 
                                 root->count, &root->body);
  arg_lr = qtree_construct_setup(root->lr, lowerright,
                                 root->count, &root->body);
  /* add task to queue */
  threadpool_add(threadpool, qtree_construct, (void *) arg_ur);
  threadpool_add(threadpool, qtree_construct, (void *) arg_ul);
  threadpool_add(threadpool, qtree_construct, (void *) arg_ll);
  threadpool_add(threadpool, qtree_construct, (void *) arg_lr);

  return root;

split_err:
  qtree_remove(&root);
root_err:
  rectangle_free(&root_range); 
root_range_err:
  return NULL;
}

void *qtree_construct(void *root_count_body) {
  /* initialize */
  qtree_construct_t *arg = (qtree_construct_t *) root_count_body;
  qtree_t *root = arg->root;
  int rectangle = arg->rectangle;
  int count = arg->count;
  body_t *(*body)[BODYMAX] = arg->body;
  qtree_construct_t *arg_ur, *arg_ul, *arg_ll, *arg_lr;
  extern threadpool_t *threadpool;
  /* pick body */
  qtree_pickbody(root, count, body, rectangle);
 
/* 
  printf("========================\n"); 
  printf("(%lf, %lf) dx=%lf dy=%lf\n", root->range->vertex->x,
                                       root->range->vertex->y,
                                       root->range->dx,
                                       root->range->dy);
  printf("count=%d\n", root->count);
  printf("========================\n"); 
*/

  /* only has 0 or 1 child */
  if (root->count == 0) {
    return NULL;
  }
  if (root->count == 1) {
    pthread_mutex_lock(&mutex);
    leaf_count++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  /* construct 4 childs */
  pthread_mutex_lock(&mutex);
  qtree_split(root);
  pthread_mutex_unlock(&mutex);
  /* set arguments for task run by thread */
  pthread_mutex_lock(&mutex);
  arg_ur = qtree_construct_setup(root->ur, upperright, 
                                 root->count, &root->body);
  arg_ul = qtree_construct_setup(root->ul, upperleft, 
                                 root->count, &root->body);
  arg_ll = qtree_construct_setup(root->ll, lowerleft, 
                                 root->count, &root->body);
  arg_lr = qtree_construct_setup(root->lr, lowerright, 
                                 root->count, &root->body);
  pthread_mutex_unlock(&mutex);
  /* add task to queue */
  threadpool_add(threadpool, qtree_construct, (void *) arg_ur);
  threadpool_add(threadpool, qtree_construct, (void *) arg_ul);
  threadpool_add(threadpool, qtree_construct, (void *) arg_ll);
  threadpool_add(threadpool, qtree_construct, (void *) arg_lr);
}

void *qtree_destruct(void *root) {
  qtree_t **r = (qtree_t **) root;
  qtree_t *ur, *ul, *ll, *lr;
  extern threadpool_t *threadpool;

  if (!(*r)) {
    return NULL;
  }

  if ((*r)->count == 0) {
    pthread_mutex_lock(&mutex);
    qtree_remove(r);
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  if ((*r)->count == 1) {
    pthread_mutex_lock(&mutex);
    leaf_count--;
    pthread_cond_signal(&cond);
    qtree_remove(r);
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  ur = (*r)->ur;
  ul = (*r)->ul;
  ll = (*r)->ll;
  lr = (*r)->lr;

  pthread_mutex_lock(&mutex);
  qtree_remove(r);
  pthread_mutex_unlock(&mutex);

  qtree_destruct((void *)&ur);
  qtree_destruct((void *)&ul);
  qtree_destruct((void *)&ll);
  qtree_destruct((void *)&lr);
/*
  threadpool_add(threadpool, qtree_destruct, (void *)&ur);
  threadpool_add(threadpool, qtree_destruct, (void *)&ul);
  threadpool_add(threadpool, qtree_destruct, (void *)&ll);
  threadpool_add(threadpool, qtree_destruct, (void *)&lr);
*/
}

/**
 * @brief wait for qtree to be done by all threads
 */
void wait_qtree(int body_num) {
  extern int leaf_count;
  pthread_mutex_lock(&mutex);
  while (leaf_count != body_num) {
    /*
    printf("leaf count: %d\n", leaf_count);
    */
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);
}

void qtree_traverse(qtree_t *root) {
  if (!root) {
    return ;
  }
  printf("========================\n"); 
  printf("(%lf, %lf) dx=%lf dy=%lf\n", root->range->vertex->x,
                                       root->range->vertex->y,
                                       root->range->dx,
                                       root->range->dy);
  printf("count=%d\n", root->count);
  printf("========================\n"); 
  qtree_traverse(root->ur);
  qtree_traverse(root->ul);
  qtree_traverse(root->ll);
  qtree_traverse(root->lr);
}

void qtree_traverse_draw_range(qtree_t *root, Display *dpy, Window w, GC gc,
                               point_t *base, double ratio, double shift ) {
  if (!root) {
    return;
  }
  double x, y, dx, dy;
  point_t *p = point_in_window(root->range->vertex, base, ratio, shift);
  x = p->x;
  y = p->y;
  dx = root->range->dx * ratio;
  dy = root->range->dy * ratio;
  XDrawRectangle(dpy, w, gc, x, y, dx, dy);
  qtree_traverse_draw_range(root->ur, dpy, w, gc, base, ratio, shift);
  qtree_traverse_draw_range(root->ul, dpy, w, gc, base, ratio, shift);
  qtree_traverse_draw_range(root->ll, dpy, w, gc, base, ratio, shift);
  qtree_traverse_draw_range(root->lr, dpy, w, gc, base, ratio, shift);
}

void qtree_destroy(qtree_t **root) {
  extern threadpool_t *threadpool;
  qtree_t *ur, *ul, *ll, *lr;

  if (!(*root)) {
    fprintf(stderr, "Error: qtree has been destroyed.\n");
    return ;
  }

  if ((*root)->count == 0) {
    qtree_remove(root);
    return ;
  }
  if ((*root)->count == 1) {
    pthread_mutex_lock(&mutex);
    leaf_count--;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    qtree_remove(root);
    return ;
  }
 
  ur = (*root)->ur;
  ul = (*root)->ul;
  ll = (*root)->ll;
  lr = (*root)->lr;

  qtree_remove(root);
  qtree_destruct((void *)&ur);
  qtree_destruct((void *)&ul);
  qtree_destruct((void *)&ll);
  qtree_destruct((void *)&lr);
/*
  threadpool_add(threadpool, qtree_destruct, (void *)&ur);
  threadpool_add(threadpool, qtree_destruct, (void *)&ul);
  threadpool_add(threadpool, qtree_destruct, (void *)&ll);
  threadpool_add(threadpool, qtree_destruct, (void *)&lr);
*/
}
