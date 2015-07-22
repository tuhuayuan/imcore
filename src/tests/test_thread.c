#include "imcore-thread.h"
#include <assert.h>
#include <stdio.h>

void messagehandle1(int msg_id, void *userdata)
{
    printf("messagehandle1\n");
    im_thread_t *current = im_thread_current();
}

void messagehandle2(int msg_id, void *userdata)
{
    im_thread_t *t1 = userdata;
    printf("messagehandle2\n");
    im_thread_t *current = im_thread_current();
    im_thread_stop(t1);
}


bool test_thread(int argc, char **argv)
{
    im_thread_init();

    im_thread_t *current = im_thread_wrap_current();
    assert(current);
    assert(im_thread_get_eventbase(current));
    im_thread_post(current, 0, messagehandle1, 5000, NULL);
    //im_thread_run(current);
    im_thread_unwrap_current(current);

    im_thread_t *t1 = im_thread_new();
    im_thread_t *t2 = im_thread_new();
    assert(t1&&t2);

    im_thread_post(t1, 0, messagehandle1, 5000, NULL);
    im_thread_post(t2, 0, messagehandle2, 5000, t1);

    im_thread_start(t1, NULL, NULL);
    im_thread_start(t2, NULL, NULL);

    im_thread_sleep(3000);
    im_thread_stop(t2);

    im_thread_free(t1);
    im_thread_free(t2);

    im_thread_destroy();
    return true;
}