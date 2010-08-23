#include "rxpriv.h"
#include <stdlib.h>

List *
list_push (List *list, void *data) {
    List *new_list, *last;
    new_list = calloc(1, sizeof (List));
    new_list->data = data;
    if (!list)
        return new_list;
    for (last = list; last->next; last = last->next)
        ;
    last->next = new_list;
    return list;
}

List *
list_copy (List *list) {
    List *item;
    List *new = NULL;
    for (item = list; item; item = item->next)
        new = list_push(new, item->data);
    return new;
}

void *
list_last_data (List *list) {
    List *last;
    if (!list)
        return NULL;
    for (last = list; last->next; last = last->next)
        ;
    return last ? last->data : NULL;
}

int
list_elems (List *list) {
    int i = 0;
    for (; list; list = list->next, i++)
        ;
    return i;
}

void *
list_nth_data (List *list, int n) {
    int i = 0;
    for (; list; list = list->next, i++) {
        if (i == n)
            return list->data;
    }
    return NULL;
}

List *
list_pop (List *list) {
    List *last, *prev;
    if (!list)
        return NULL;
    for (prev = NULL, last = list; last->next; prev = last, last = last->next)
        ;
    if (prev)
        prev->next = NULL;
    free(last);
    if (last == list)
        return NULL;
    return list;
}

List *
list_cat (List *a, List *b) {
    List *last;
    if (!a)
        return b;
    if (!b)
        return a;
    for (last = a; last->next; last = last->next)
        ;
    last->next = b;
    return a;
}

void
list_free (List *list, void (*freefunc) ()) {
    List *item, *next;
    for (item = list; item; item = next) {
        next = item->next;
        if (freefunc)
            freefunc(item->data);
        free(item);
    }
}

List *
list_remove (List *list, void *data, int (*cmpfunc) (), void (*freefunc) ()) {
    List *tmp, *prev;
    for (prev = NULL, tmp = list; tmp; prev = tmp, tmp = tmp->next) {
        if (cmpfunc ? cmpfunc(tmp->data, data) : (tmp->data != data))
            continue;
        if (prev)
            prev->next = tmp->next;
        else
            list = tmp->next;
        tmp->next = NULL;
        list_free(tmp, freefunc);
        tmp = prev;
        if (!tmp)
            break;
    }
    return list;
}

List *
list_find (List *list, void *data, int (*cmpfunc) ()) {
    List *item;
    for (item = list; item; item = item->next) {
        if (cmpfunc ? !cmpfunc(item->data, data) : (item->data == data))
            return item;
    }
    return NULL;
}

