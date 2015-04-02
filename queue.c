#include "queue.h"


queue_t *queue_middle(queue_t *queue)
{
    queue_t  *middle, *next;

    middle = queue_head(queue);

    if (middle == queue_last(queue))
    {
        return middle;
    }

    next = queue_head(queue);

    for ( ;; )
    {
        middle = queue_next(middle);

        next = queue_next(next);

        if (next == queue_last(queue))
        {
            return middle;
        }

        next = queue_next(next);

        if (next == queue_last(queue))
        {
            return middle;
        }
    }
}


/* the stable insertion sort */

void
queue_sort(queue_t *queue,
           int (*cmp)(const queue_t *, const queue_t *))
{
    queue_t  *q, *prev, *next;

    q = queue_head(queue);

    if (q == queue_last(queue))
    {
        return;
    }

    for (q = queue_next(q); q != queue_sentinel(queue); q = next)
    {

        prev = queue_prev(q);
        next = queue_next(q);

        queue_remove(q);

        do
        {
            if (cmp(prev, q) <= 0)
            {
                break;
            }

            prev = queue_prev(prev);

        }
        while (prev != queue_sentinel(queue));

        queue_insert_after(prev, q);
    }
}
