

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"




/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {

    //TODO: add your code here
    dplist_node_t *current;
    dplist_node_t *next_node;

    if (list == NULL || *list == NULL) {
        return;
    }

    current = (*list)->head;

    while (current != NULL) {
        next_node = current->next; // Save next node before freeing current

        if (free_element) {
            // Use the callback function to free the element
            (*list)->element_free(&(current->element));
        }

        free(current);
        current = next_node;
    }

    free(*list);
    *list = NULL;

}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    //TODO: add your code here
    dplist_node_t *new_node, *current_node;
    int current_index;

    if (list == NULL)
    {
        return NULL;
    }

    new_node = malloc(sizeof(dplist_node_t));
    if (new_node == NULL)
    {
        return list; // Malloc failed, return original list
    }

    if (insert_copy) {
        new_node->element = list->element_copy(element);
    } else {
        new_node->element = element;
    }
    new_node->prev = NULL;
    new_node->next = NULL;

    if (list->head == NULL) {
        // Case A: List is empty
        list->head = new_node;
    } else if (index <= 0) {
        // Case B: Insert at head
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
    } else {
        // Case C/D: Insert in middle or at end
        current_node = list->head;
        current_index = 0;

        // Traverse to the node *before* the insertion point (or the last node)
        while (current_node->next != NULL && current_index < index - 1) {
            current_node = current_node->next;
            current_index++;
        }

        if (current_node->next == NULL) {
            // Case D: Insert at end (index >= size)
            current_node->next = new_node;
            new_node->prev = current_node;
        } else {
            // Case C: Insert in middle (at index)
            new_node->next = current_node->next;
            new_node->prev = current_node;
            current_node->next->prev = new_node;
            current_node->next = new_node;
        }
    }

    return list;

}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {

    //TODO: add your code here
    dplist_node_t *node_to_remove;

    if (list == NULL) {
        return NULL;
    }
    if (list->head == NULL) {
        return list;
    }

    node_to_remove = dpl_get_reference_at_index(list, index);

    if (node_to_remove == list->head) {
        // Case A: Removing the head
        list->head = node_to_remove->next;
        if (list->head != NULL) {
            list->head->prev = NULL;
        }
    } else if (node_to_remove->next == NULL) {
        // Case B: Removing the tail (node has a 'prev' but no 'next')
        node_to_remove->prev->next = NULL;
    } else {
        // Case C: Removing from the middle
        node_to_remove->prev->next = node_to_remove->next;
        node_to_remove->next->prev = node_to_remove->prev;
    }

    if (free_element) {
        list->element_free(&(node_to_remove->element));
    }

    free(node_to_remove);
    return list;
}

int dpl_size(dplist_t *list) {

    //TODO: add your code here
    int count = 0;
    dplist_node_t *current;

    if (list == NULL) {
        return -1;
    }

    current = list->head;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;

}

void *dpl_get_element_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    dplist_node_t *node;
    node = dpl_get_reference_at_index(list, index);

    if (node == NULL) {
        return NULL;
    }

    return node->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {

    //TODO: add your code here
    int index = 0;
    dplist_node_t *current;

    if (list == NULL) {
        return -1;
    }

    current = list->head;
    while (current != NULL) {
        if (list->element_compare(current->element, element) == 0) {
            return index;
        }
        current = current->next;
        index++;
    }

    return -1;

}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    int count = 0;
    dplist_node_t *current;

    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    if (index <= 0) {
        return list->head;
    }

    current = list->head;

    while (current->next != NULL && count < index) {
        current = current->next;
        count++;
    }

    return current;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {

    //TODO: add your code here
    if (list == NULL || reference == NULL)
    {
        return NULL;
    }

    dplist_node_t *current = list->head;
    while (current != NULL) {
        if (current == reference) {
            return current->element;
        }
        current = current->next;
    }

    return NULL;
}


