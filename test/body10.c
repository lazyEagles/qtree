
#include <X11/Xlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "qtree.h"
#include "threadpool.h"

#define THREAD 4
#define QUEUE 4096

pthread_mutex_t mutex;
pthread_cond_t cond;
threadpool_t *threadpool;

void read_data(char *fn, int *count, body_t *(*body)[BODYMAX]); 

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Use: ./body10 filename\n");
    return 0;
  }
  /* initial lock and threadpool */
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
  threadpool = threadpool_create(THREAD, QUEUE);

  int count;
  body_t *body[BODYMAX];
  qtree_t *root;
  int i;

  read_data(argv[1], &count, &body);
  /* create a qtree */
  root = qtree_create(count, &body);
  wait_qtree(count);
  /* traverse the qtree */
  qtree_traverse(root);

  /* ====== x11 ======= */
  Display *dpy = XOpenDisplay(NULL);
  assert(dpy);
  int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
  int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

  Window w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                 400, 400, 0, blackColor, blackColor);

/*
  XSelectInput(dpy, w, StructureNotifyMask);
*/

  XSelectInput(dpy, w, ExposureMask | KeyPressMask);
  XMapWindow(dpy, w);

  GC gc = XCreateGC(dpy, w, 0, NULL);
  
  XSetForeground(dpy, gc, whiteColor);

  for(;;) {
    XEvent e;
    XNextEvent(dpy, &e);
    if (e.type == Expose) {
      /*
      XDrawLine(dpy, w, gc, 10, 60, 180, 20);
      XFillRectangle(dpy, w, gc, 20, 20, 10, 10);
      XDrawPoint(dpy, w, gc, 200, 200);
      XDrawRectangle(dpy, w, gc, 40, 40, 320, 320);
      */
      /* draw body */
      for (i = 0; i < count; i++) {
        point_t *p;
        p = point_in_window(body[i]->pos, root->range->vertex, 
                            320.0/root->range->dx, 40);
        XDrawPoint(dpy, w, gc, p->x, p->y);
        point_free(&p);
      }
      /* draw rectangle */
      qtree_traverse_draw_range(root, dpy, w, gc, root->range->vertex, 
                                320.0/root->range->dx, 40);
    }
    if (e.type == KeyPress) {
      break;
    }
    if (e.type == ClientMessage) {
      break;
    }
    if (e.type == MapNotify) {
      break;
    }
  }
/*
  XFlush(dpy);
  sleep(10);
*/
  XDestroyWindow(dpy, w);
  XCloseDisplay(dpy);
  /* ====== x11 ======= */

  /* destory the qtree */
  qtree_destroy(&root);
  printf("root: %p\n", root);
  /* destroy thread pool */
  threadpool_destroy(threadpool);
  /* destroy body */
  for (i = 0; i < count; i++) {
    body_free(&body[i]);
    printf("body: %p\n", body[i]);
  } 
  /* destroy lock */
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  return 0;
}

/**
 * @brief read body info from file into body array 
 */
void read_data(char *fn, int *count, body_t *(*body)[BODYMAX]) {
  FILE *fd;
  int i;
  double x, y, mass;

  fd = fopen(fn, "r");
  if (!fd) {
    fprintf(stderr, "Error: fail to open file.\n");
    exit(1);
  }
  fscanf(fd, "%d", count);
  for (i = 0; i < *count; i++) {
    fscanf(fd, "%lf%lf%lf", &x, &y, &mass);
    (*body)[i] = body_create(x, y, mass);
    printf("%lf %lf %lf\n", (*body)[i]->pos->x, (*body)[i]->pos->y, 
                            (*body)[i]->mass);
  }
  fclose(fd);
}
