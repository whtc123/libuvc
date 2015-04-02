#ifndef _queue_H_INCLUDED_
#define _queue_H_INCLUDED_

struct queue_s
{
    struct queue_s *prev;
    struct queue_s *next;
    void *ext;
};
typedef struct queue_s  queue_t;




#define queue_init(q)                                                     \
    (q)->prev = q;                                                            \
    (q)->next = q


#define queue_empty(h)                                                    \
    (h == (h)->prev)


#define queue_insert_head(h, x)                                           \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x


#define queue_insert_after   queue_insert_head


#define queue_insert_tail(h, x)                                           \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


#define queue_head(h)                                                     \
    (h)->next


#define queue_last(h)                                                     \
    (h)->prev


#define queue_sentinel(h)                                                 \
    (h)


#define queue_next(q)                                                     \
    (q)->next


#define queue_prev(q)                                                     \
    (q)->prev


#if (NGX_DEBUG)

#define queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

#define queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif


#define queue_split(h, q, n)                                              \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


#define queue_add(h, n)                                                   \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


#define queue_data(q, type, link)                                         \
    (type *) ((uint8_t *) q - offsetof(type, link))


queue_t *queue_middle(queue_t *queue);
void queue_sort(queue_t *queue, int (*cmp)(const queue_t *, const queue_t *));


#endif /* _queue_H_INCLUDED_ */
