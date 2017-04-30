/*source: https://rosettacode.org/wiki/AVL_tree/C */ 
/* extended by Salvatore Di Girolamo */

#ifndef AVLTREE_INCLUDED
#define AVLTREE_INCLUDED
typedef struct Tree *Tree;
typedef struct Node *Node;
 
Tree  Tree_New        (int (*comp)(void *, void *), void (*print)(void *, void *), void (*user_replace)(void *, void *), void * context, int totnodes);
 
Node  Tree_Insert     (Tree t, void *data);
void  Tree_DeleteNode (Tree t, Node  node);
Node  Tree_SearchNode (Tree t, void *data);
void *  Tree_CSearchNode (Tree t, void *data); //ceil

void Tree_Empty(Tree t);
 
Node  Tree_FirstNode  (Tree t);
Node  Tree_LastNode   (Tree t);
 
Node  Tree_PrevNode   (Tree t, Node n);
Node  Tree_NextNode   (Tree t, Node n);
 
void  Tree_Print      (Tree t);
 
void *Node_GetData (Node n);

int Tree_CountNodes(Tree t);

 
#endif
