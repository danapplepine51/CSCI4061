/**
 * p_queue.h
 *
 * by: Daniel Song, Isaac Stein
 *
 * last modified: 2018 Apr. 12
 *
 * This file details the structure of a priority queue and its underlying node implemenation.
 * All necessary standard functions for accessing the priority queue are included in this file.
 * The priority queue as discussed in this file implements the min-heap strategy using an
 *  an underlying heap structure.
 */

/********************************************************************
 * Includes
 *******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************
 * Structures
 *******************************************************************/

struct pq_node{
    struct pq_node *left;
    struct pq_node *right;
    char *name;
    int priority;
};

struct p_queue{
    struct pq_node *head;
};

/*******************************************************************
 * Constructors
 ******************************************************************/

/**
 * @brief The default constructor for the priority queue.
 *
 * @return A new empty priority queue.
 */
struct p_queue *NewQueue() {
    struct p_queue *new_struct = malloc(sizeof(struct p_queue));
    new_struct->head = NULL;
    return new_struct;
};


/*******************************************************************
 * Private Functions
 ******************************************************************/
// Is there a way to inhibit other files from calling these functions in C?

/**
 * @brief The new node sinks down to its proper place in the queue.
 *        This function utilizes a messy implementation that does not
 *          minimize queue depth.
 *
 * @param A pointer to the pointer that leads to the current node.
 * @param A pointer to the node to be added to the queue.
 */
void sink_down(struct pq_node **queue_node, struct pq_node *new_node) {
    if (*queue_node == NULL) {
        *queue_node = new_node;
        return;
    }

    if (new_node->priority < (*queue_node)->priority) {
        new_node->left = *queue_node;
        *queue_node = new_node;
        return;
    }

    if ((*queue_node)->left == NULL) {
        (*queue_node)->left = new_node;
        return;
    }
    if ((*queue_node)->right == NULL) {
        (*queue_node)->right = new_node;
        return;
    }

    int left_priority, right_priority;
    left_priority = (*queue_node)->left->priority;
    right_priority = (*queue_node)->right->priority;
    if (left_priority <= right_priority) {
        sink_down(&((*queue_node)->left), new_node);
    } else {
        sink_down(&((*queue_node)->right), new_node);
    }
}

/**
 * @brief Reorder a queue in order to maintain the min-heap property.
 *        This function is recursively called on every node that needs
 *          that may need to reorder its children.
 *        If a node has a free child slot then it may accept more children.
 *        Elsewise, a child is reordered to free up a slot, which the parent gives its sibling to.
 *
 * @param The node that is to be reordered.
 */
void reorder(struct pq_node *node) {
    // There are two children, the larger one is reordered
    //   and its sibling is set as its free child.
    if  (node->left != NULL && node->right != NULL) {
        int left_priority, right_priority;
        struct pq_node *reordered_node;

        left_priority = node->left->priority;
        right_priority = node->right->priority;

        if (left_priority <= right_priority) {
            reordered_node = node->left;
            reorder(node->left);
            if (reordered_node->left == NULL) reordered_node->left = node->right;
            else if (reordered_node->right == NULL) reordered_node->right = node->right;
            node->right = NULL;
        } else {
            reordered_node = node->right;
            reorder(node->right);
            if (reordered_node->left == NULL) reordered_node->left = node->left;
            else if (reordered_node->right == NULL) reordered_node->right = node->left;
            node->left = NULL;
        }
    }
}

/*******************************************************************
 * Public Functions
 ******************************************************************/
/**
 * @brief Prints all of the numbers in a priority queue according to parent-child ordering.
 */
void print_queue_node(struct pq_node *node, int depth) {
    if (node == NULL) {
        for(int i=0 ; i<depth ; i++) printf("  ");
        printf("[]\n");
        return;
    }
    print_queue_node(node->left, depth+1);
    for(int i=0 ; i<depth ; i++) printf("  ");
    printf("[%d]\n", node->priority);
    print_queue_node(node->right, depth+1);
}

/**
 * @brief Prints a priority queue.
 */
void print_queue(struct p_queue *queue) {
    printf("[\n");
    print_queue_node(queue->head, 1);
    printf("]\n");
}

/**
 * @brief Enqueues an new element into a priority queue.
 *        The new element is automatically sorted according
 *          to its priority.
 *
 * @param A pointer to the priority queue.
 * @param The name of the element to be added.
 * @param The priority of the element to be added.
 */
void enqueue(struct p_queue *queue, char *name, int priority) {
    struct pq_node *new_node = malloc(sizeof(struct pq_node));
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->name = name;
    new_node->priority = priority;

    sink_down(&(queue->head), new_node);
}

/**
 * @brief Dequeues the first element from a priority queue.
 *        The priority queue is constructed using a max-heap structure.
 *        This function modifies the max-head to remove the element
 *          and to reorder the heap according to the max-heap rules.
 *
 * @param The heap to remove an element from.
 *
 * @return The name of the element removed.
 */
char *dequeue(struct p_queue *queue) {
    if (queue->head == NULL) return NULL;
    char *return_string = malloc(strlen(queue->head->name) * sizeof(char));
    struct pq_node *node_to_remove = queue->head;

    strcpy(return_string, node_to_remove->name);

    reorder(queue->head);

    if (queue->head->left != NULL) queue->head = queue->head->left;
    else queue->head = queue->head->right;

    free(node_to_remove);

    return return_string;
}
