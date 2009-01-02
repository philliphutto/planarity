/*
Planarity-Related Graph Algorithms Project
Copyright (c) 1997-2009, John M. Boyer
All rights reserved. Includes a reference implementation of the following:
John M. Boyer and Wendy J. Myrvold, "On the Cutting Edge: Simplified O(n)
Planarity by Edge Addition,"  Journal of Graph Algorithms and Applications,
Vol. 8, No. 3, pp. 241-273, 2004.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of the Planarity-Related Graph Algorithms Project nor the names
  of its contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define GRAPH_C

#include <stdlib.h>

#include "graph.h"

/* Imported functions */

extern void _FillVisitedFlags(graphP, int);

extern int _IsolateKuratowskiSubgraph(graphP theGraph, int I);
extern int _IsolateOuterplanarObstruction(graphP theGraph, int I);

/* Private functions (some are exported to system only) */

void _CreateSortedSeparatedDFSChildLists(graphP theGraph);
int  _CreateFwdArcLists(graphP theGraph);
void _CreateDFSTreeEmbedding(graphP theGraph);

void _EmbedBackEdgeToDescendant(graphP theGraph, int RootSide, int RootVertex, int W, int WPrevLink);

int  _GetNextVertexOnExternalFace(graphP theGraph, int curVertex, int *pPrevLink);

void _InvertVertex(graphP theGraph, int V);
void _MergeVertex(graphP theGraph, int W, int WPrevLink, int R);
int  _MergeBicomps(graphP theGraph, int I, int RootVertex, int W, int WPrevLink);

void _WalkUp(graphP theGraph, int I, int J);
int  _WalkDown(graphP theGraph, int I, int RootVertex);

int  _EmbedIterationPostprocess(graphP theGraph, int I);
int  _EmbedPostprocess(graphP theGraph, int I, int edgeEmbeddingResult);

void _OrientVerticesInEmbedding(graphP theGraph);
void _OrientVerticesInBicomp(graphP theGraph, int BicompRoot, int PreserveSigns);
int  _JoinBicomps(graphP theGraph);

/********************************************************************
 _CreateSortedSeparatedDFSChildLists()
 We create a separatedDFSChildList in each vertex which contains the
 Lowpoint values of the vertex's DFS children sorted in non-descending order.
 To accomplish this in linear time for the whole graph, we must not
 sort the DFS children in each vertex, but rather bucket sort the
 Lowpoint values of all vertices, then traverse the buckets sequentially,
 adding each vertex to its parent's separatedDFSChildList.
 Note that this is a specialized bucket sort that achieves O(n)
 worst case rather than O(n) expected time due to the simplicity
 of the sorting problem.  Specifically, we know that the Lowpoint values
 are between 0 and N-1, so we create buckets for each value.
 Collisions occur only when two keys are equal, so there is no
 need to sort the buckets (hence O(n) worst case).
 ********************************************************************/

void  _CreateSortedSeparatedDFSChildLists(graphP theGraph)
{
int *buckets;
listCollectionP bin;
int I, J, N, DFSParent, theList;

     N = theGraph->N;
     buckets = theGraph->buckets;
     bin = theGraph->bin;

     /* Initialize the bin and all the buckets to be empty */

     LCReset(bin);
     for (I=0; I < N; I++)
          buckets[I] = NIL;

     /* For each vertex, add it to the bucket whose index is equal to
        the Lowpoint of the vertex. */

     for (I=0; I < N; I++)
     {
          J = theGraph->V[I].Lowpoint;
          buckets[J] = LCAppend(bin, buckets[J], I);
     }

     /* For each bucket, add each vertex in the bucket to the
        separatedDFSChildList of its DFSParent.  Since lower numbered buckets
        are processed before higher numbered buckets, vertices with lower
        Lowpoint values are added before those with higher Lowpoint values,
        so the separatedDFSChildList of each vertex is sorted by Lowpoint */

     for (I = 0; I < N; I++)
     {
          if ((J=buckets[I]) != NIL)
          {
              while (J != NIL)
              {
                  DFSParent = theGraph->V[J].DFSParent;

                  if (DFSParent != NIL && DFSParent != J)
                  {
                      theList = theGraph->V[DFSParent].separatedDFSChildList;
                      theList = LCAppend(theGraph->DFSChildLists, theList, J);
                      theGraph->V[DFSParent].separatedDFSChildList = theList;
                  }

                  J = LCGetNext(bin, buckets[I], J);
              }
          }
     }
}

/********************************************************************
 _CreateFwdArcLists()

 Puts the forward arcs (back edges from a vertex to its descendants)
 into a circular list indicated by the fwdArcList member, a task
 simplified by the fact that they have already been placed in link[1]
 succession.

  Returns OK for success, NOTOK for internal code failure
 ********************************************************************/

int _CreateFwdArcLists(graphP theGraph)
{
int I, Jfirst, Jnext, Jlast;

    // For each vertex, the forward arcs are already in succession
    // along the link[1] pointers...

    for (I=0; I < theGraph->N; I++)
    {
    	// Skip this vertex if it has no edges
    	if (!((Jfirst = theGraph->G[I].link[1]) >= theGraph->edgeOffset))
    		continue;

        // If the vertex has any forward edges, they will be in link[1]
        // succession.  So we test if we have a forward edge, then ...

        if (theGraph->G[Jfirst].type == EDGE_FORWARD)
        {
            // Find the end of the forward edge list

            Jnext = Jfirst;
            while (theGraph->G[Jnext].type == EDGE_FORWARD)
                Jnext = theGraph->G[Jnext].link[1];
            Jlast = theGraph->G[Jnext].link[0];

            // Remove the forward edges from the adjacency list of I

            theGraph->G[Jnext].link[0] = I;
            theGraph->G[I].link[1] = Jnext;

            // Make a circular forward edge list

            theGraph->V[I].fwdArcList = Jfirst;
            theGraph->G[Jfirst].link[0] = Jlast;
            theGraph->G[Jlast].link[1] = Jfirst;
        }
    }

    return OK;
}

/********************************************************************
 ********************************************************************/

#ifdef DEBUG
int  TestIntegrity(graphP theGraph)
{
    int I, Jcur, result = 1;

        for (I=0; I < theGraph->N; I++)
        {
            Jcur = theGraph->V[I].fwdArcList;
            while (Jcur != NIL)
            {
                if (theGraph->G[Jcur].visited)
                {
                    printf("Found problem with fwdArcList of vertex %d.\n", I);
                    result = 0;
                    break;
                }

                theGraph->G[Jcur].visited = 1;

                Jcur = theGraph->G[Jcur].link[0];
                if (Jcur == theGraph->V[I].fwdArcList)
                    Jcur = NIL;
            }

            Jcur = theGraph->V[I].fwdArcList;
            while (Jcur != NIL)
            {
                if (!theGraph->G[Jcur].visited)
                    break;

                theGraph->G[Jcur].visited = 0;

                Jcur = theGraph->G[Jcur].link[0];
                if (Jcur == theGraph->V[I].fwdArcList)
                    Jcur = NIL;
            }
        }

        return result;
}
#endif

/********************************************************************
 _CreateDFSTreeEmbedding()

 Each vertex receives only its parent arc in the adjacency list, and
 the corresponding child arc is placed in the adjacency list of a root
 copy of the parent.  Each root copy of a vertex is uniquely associated
 with a child C, so it is simply stored at location C+N.

 The forward arcs are not lost because they are already in the
 fwdArcList of each vertex.  Each back arc can be reached as the
 twin arc of a forward arc, and the two are embedded together when
 the forward arc is processed.  Finally, the child arcs are initially
 placed in root copies of vertices, not the vertices themselves, but
 the child arcs are merged into the vertices as the embedder progresses.
 ********************************************************************/

void _CreateDFSTreeEmbedding(graphP theGraph)
{
int N, I, J, Jtwin, R;

    N = theGraph->N;

    // Embed all tree edges.  For each DFS tree child, we move
    // the child arc to a root copy of vertex I that is uniquely
    // associated with the DFS child, and we remove all edges
    // from the child except the parent arc

    for (I=0, R=N; I < N; I++, R++)
    {
        if (theGraph->V[I].DFSParent == NIL)
        {
            theGraph->G[I].link[0] = theGraph->G[I].link[1] = I;
        }
        else
        {
            J = theGraph->G[I].link[0];
            while (theGraph->G[J].type != EDGE_DFSPARENT)
                J = theGraph->G[J].link[0];

            theGraph->G[I].link[0] = theGraph->G[I].link[1] = J;
            theGraph->G[J].link[0] = theGraph->G[J].link[1] = I;
            theGraph->G[J].v = R;

            Jtwin = gp_GetTwinArc(theGraph, J);

            theGraph->G[R].link[0] = theGraph->G[R].link[1] = Jtwin;
            theGraph->G[Jtwin].link[0] = theGraph->G[Jtwin].link[1] = R;

            theGraph->extFace[R].link[0] = theGraph->extFace[R].link[1] = I;
            theGraph->extFace[I].link[0] = theGraph->extFace[I].link[1] = R;
        }
    }
}

/********************************************************************
 _EmbedBackEdgeToDescendant()
 The Walkdown has found a descendant vertex W to which it can
 attach a back edge up to the root of the bicomp it is processing.
 The RootSide and WPrevLink indicate the parts of the external face
 that will be replaced at each endpoint of the back edge.
 ********************************************************************/

void _EmbedBackEdgeToDescendant(graphP theGraph, int RootSide, int RootVertex, int W, int WPrevLink)
{
int fwdArc, backArc, parentCopy;

    /* We get the two edge records of the back edge to embed.
        The Walkup recorded in W's adjacentTo the index of the forward arc
        from the root's parent copy to the descendant W. */

    fwdArc = theGraph->V[W].adjacentTo;
    backArc = gp_GetTwinArc(theGraph, fwdArc);

    /* The forward arc is removed from the fwdArcList of the root's parent copy. */

    parentCopy = theGraph->V[RootVertex - theGraph->N].DFSParent;

    if (theGraph->V[parentCopy].fwdArcList == fwdArc)
    {
        if (theGraph->G[fwdArc].link[0] == fwdArc)
             theGraph->V[parentCopy].fwdArcList = NIL;
        else theGraph->V[parentCopy].fwdArcList = theGraph->G[fwdArc].link[0];
    }

    theGraph->G[theGraph->G[fwdArc].link[0]].link[1] = theGraph->G[fwdArc].link[1];
    theGraph->G[theGraph->G[fwdArc].link[1]].link[0] = theGraph->G[fwdArc].link[0];

    /* The forward arc is added to the adjacency list of the RootVertex. */

    theGraph->G[fwdArc].link[1^RootSide] = RootVertex;
    theGraph->G[fwdArc].link[RootSide] = theGraph->G[RootVertex].link[RootSide];
    theGraph->G[theGraph->G[RootVertex].link[RootSide]].link[1^RootSide] = fwdArc;
    theGraph->G[RootVertex].link[RootSide] = fwdArc;

    /* The back arc is added to the adjacency list of W. */

    theGraph->G[backArc].v = RootVertex;

    theGraph->G[backArc].link[1^WPrevLink] = W;
    theGraph->G[backArc].link[WPrevLink] = theGraph->G[W].link[WPrevLink];
    theGraph->G[theGraph->G[W].link[WPrevLink]].link[1^WPrevLink] = backArc;
    theGraph->G[W].link[WPrevLink] = backArc;

    /* Link the two endpoint vertices together on the external face */

    theGraph->extFace[RootVertex].link[RootSide] = W;
    theGraph->extFace[W].link[WPrevLink] = RootVertex;
}

/********************************************************************
 _GetNextVertexOnExternalFace()
 Each vertex contains a link[0] and link[1] that link it into its
 list of edges.  If the vertex is on the external face, then the two
 edge nodes pointed to by link[0] and link[1] are also on the
 external face.  We want to take one of those edges to get to the
 next vertex on the external face.
 On input *pPrevLink indicates which link we followed to arrive at
 curVertex.  On output *pPrevLink will be set to the link we follow to
 get into the next vertex.
 To get to the next vertex, we use the opposite link from the one used
 to get into curVertex.  This takes us to an edge node.  The twinArc
 of that edge node, carries us to an edge node in the next vertex.
 At least one of the two links in that edge node will lead to a vertex
 node in G, which is the next vertex.  Once we arrive at the next
 vertex, at least one of its links will lead back to the edge node, and
 that link becomes the output value of *pPrevLink.
 ********************************************************************/

int  _GetNextVertexOnExternalFace(graphP theGraph, int curVertex, int *pPrevLink)
{
int  arc, nextArc, nextVertex, newPrevLink;

     /* Exit curVertex from whichever link was not previously used to enter it */

     arc = theGraph->G[curVertex].link[1^(*pPrevLink)];

     nextArc = gp_GetTwinArc(theGraph, arc);

     nextVertex = theGraph->G[nextArc].link[newPrevLink=0];
     if (nextVertex >= theGraph->edgeOffset)
         nextVertex = theGraph->G[nextArc].link[newPrevLink=1];

     /* The setting above is how we exited an edge record to get to the
        next vertex.  The reverse pointer leads back from the vertex to
        the edge record. */

     newPrevLink = 1^newPrevLink;

     /* This if stmt assigns the new prev link that tells us which edge
        record was used to enter nextVertex (so that we exit from the
        opposing edge record).
        However, if we are in a singleton bicomp, then both links in nextVertex
        lead back to curVertex, so newPrevLink may get stop at the zero setting
        when it should become one.
        We want the two arcs of a singleton bicomp to act like a cycle, so the
        edge record given as the prev link for curVertex should be the same as
        the prev link for nextVertex.
        So, we only need to modify the prev link if the links in nextVertex
        are not equal. */

     if (theGraph->G[nextVertex].link[0] != theGraph->G[nextVertex].link[1])
         *pPrevLink = newPrevLink;

     return nextVertex;
}

/********************************************************************
 _InvertVertex()
 This function flips the orientation of a single vertex such that
 instead of using link[0] successors to go clockwise (or counterclockwise)
 around a vertex's adjacency list, link[1] successors would be used.
 The loop is constructed using do-while so we can swap the links
 in the vertex node as well as each arc node.
 ********************************************************************/

void _InvertVertex(graphP theGraph, int V)
{
int J, JTemp;

     J = V;
     do {
        JTemp = theGraph->G[J].link[0];
        theGraph->G[J].link[0] = theGraph->G[J].link[1];
        theGraph->G[J].link[1] = JTemp;

        J = theGraph->G[J].link[0];
     }  while (J >= theGraph->edgeOffset);

     JTemp = theGraph->extFace[V].link[0];
     theGraph->extFace[V].link[0] = theGraph->extFace[V].link[1];
     theGraph->extFace[V].link[1] = JTemp;
}

/********************************************************************
 _MergeVertex()
 The merge step joins the vertex W to the root R of a child bicompRoot,
 which is a root copy of W appearing in the region N to 2N-1.

 Actually, the first step of this is to redirect all of the edges leading
 into R so that they indicate W as the neighbor instead of R.
 For each edge node pointing to R, we set the 'v' field to W.  Once an
 edge is redirected from a root copy R to a parent copy W, the edge is
 never redirected again, so we associate the cost of the redirection
 as constant per edge, which maintains linear time performance.

 After this is done, a regular circular list union occurs. The only
 consideration is that WPrevLink is used to indicate the two edge
 records e_w and e_r that will become consecutive in the resulting
 adjacency list of W.  We set e_w to W's link[WPrevLink] and e_r to
 R's link[1^WPrevLink] so that e_w and e_r indicate W and R with
 opposing links, which become free to be cross-linked.  Finally,
 the edge record e_ext, set equal to R's link[WPrevLink], is the edge
 that, with e_r, held R to the external face.  Now, e_ext will be the
 new link[WPrevLink] edge record for W.  If e_w and e_r become part
 of a proper face, then e_ext and W's link[1^WPrevLink] are the two
 edges that hold W to the external face.
 ********************************************************************/

void _MergeVertex(graphP theGraph, int W, int WPrevLink, int R)
{
int  J, JTwin, edgeOffset;
int  e_w, e_r, e_ext;

     edgeOffset = theGraph->edgeOffset;

     /* All arcs leading into R from its neighbors must be changed
        to say that they are leading into W */

     J = theGraph->G[R].link[0];
     while (J >= edgeOffset)
     {
         JTwin = gp_GetTwinArc(theGraph, J);
         theGraph->G[JTwin].v = W;

         J = theGraph->G[J].link[0];
     }

     /* Obtain the edge records involved in the circular list union */

     e_w = theGraph->G[W].link[WPrevLink];
     e_r = theGraph->G[R].link[1^WPrevLink];
     e_ext = theGraph->G[R].link[WPrevLink];

     /* WPrevLink leads away from W to e_w, so 1^WPrevLink in e_w leads back to W.
        Now it must lead to e_r.  Likewise, e_r needs to lead back to e_w
        with the opposing link, which is link[WPrevLink] */

     theGraph->G[e_w].link[1^WPrevLink] = e_r;
     theGraph->G[e_r].link[WPrevLink] = e_w;

     /* Now we cross-link W's link[WPrevLink] and link[1^WPrevLink] in the
        edge record e_ext */

     theGraph->G[W].link[WPrevLink] = e_ext;
     theGraph->G[e_ext].link[1^WPrevLink] = W;

     /* Erase the entries in R, which is a root copy that is no longer needed. */

     theGraph->functions.fpInitGraphNode(theGraph, R);
}

/********************************************************************
 _MergeBicomps()

 Merges all biconnected components at the cut vertices indicated by
 entries on the stack.

 theGraph contains the stack of bicomp roots and cut vertices to merge

 I, RootVertex, W and WPrevLink are not used in this routine, but are
          used by overload extensions

 Returns OK, but an extension function may return a value other than
         OK in order to cause Walkdown to terminate immediately.
********************************************************************/

int  _MergeBicomps(graphP theGraph, int I, int RootVertex, int W, int WPrevLink)
{
int  R, Rout, Z, ZPrevLink, J;
int  theList, RootID_DFSChild;
int  extFaceVertex;

     while (sp_NonEmpty(theGraph->theStack))
     {
         sp_Pop2(theGraph->theStack, R, Rout);
         sp_Pop2(theGraph->theStack, Z, ZPrevLink);

         /* The external faces of the bicomps containing R and Z will
            form two corners at Z.  One corner will become part of the
            internal face formed by adding the new back edge. The other
            corner will be the new external face corner at Z.
            We first want to update the links at Z to reflect this. */

         extFaceVertex = theGraph->extFace[R].link[1^Rout];
         theGraph->extFace[Z].link[ZPrevLink] = extFaceVertex;

         if (theGraph->extFace[extFaceVertex].link[0] == theGraph->extFace[extFaceVertex].link[1])
            theGraph->extFace[extFaceVertex].link[Rout ^ theGraph->extFace[extFaceVertex].inversionFlag] = Z;
         else
            theGraph->extFace[extFaceVertex].link[theGraph->extFace[extFaceVertex].link[0] == R ? 0 : 1] = Z;

         /* If the path used to enter Z is opposed to the path
            used to exit R, then we have to flip the bicomp
            rooted at R, which we signify by inverting R
            then setting the sign on its DFS child edge to
            indicate that its descendants must be flipped later */

         if (ZPrevLink == Rout)
         {
             Rout = 1^ZPrevLink;

             if (theGraph->G[R].link[0] != theGraph->G[R].link[1])
                _InvertVertex(theGraph, R);

             J = theGraph->G[R].link[0];
             while (J >= theGraph->edgeOffset)
             {
                 if (theGraph->G[J].type == EDGE_DFSCHILD)
                 {
                	 SET_EDGEFLAG_INVERTED(theGraph, J);
                     break;
                 }

                 J = theGraph->G[J].link[0];
             }
         }

         // The endpoints of a bicomp's "root edge" are the bicomp root R and a
         // DFS child of the parent copy of the bicomp root R.
         // The GraphNode location of the root vertices is in the range N to 2N-1
         // at the offset indicated by the associated DFS child.  So, the location
         // of the root vertex R, less N, is the location of the DFS child and also
         // a convenient identifier for the bicomp root.
         RootID_DFSChild = R - theGraph->N;

         /* R is no longer pertinent to Z since we are about to
            merge R into Z, so we delete R from its pertinent
            bicomp list (Walkdown gets R from the head of the list). */

         theList = theGraph->V[Z].pertinentBicompList;
         theList = LCDelete(theGraph->BicompLists, theList, RootID_DFSChild);
         theGraph->V[Z].pertinentBicompList = theList;

         /* As a result of the merge, the DFS child of Z must be removed
            from Z's SeparatedDFSChildList because the child has just
            been joined directly to Z, rather than being separated by a
            root copy. */

         theList = theGraph->V[Z].separatedDFSChildList;
         theList = LCDelete(theGraph->DFSChildLists, theList, RootID_DFSChild);
         theGraph->V[Z].separatedDFSChildList = theList;

         /* Now we push R into Z, eliminating R */

         _MergeVertex(theGraph, Z, ZPrevLink, R);
     }

     return OK;
}

/********************************************************************
 _WalkUp()
 I is the vertex currently being embedded
 J is the forward arc to the descendant W on which the Walkup begins

 The Walkup establishes pertinence for step I.  It marks W as
 'adjacentTo' I so that the Walkdown will embed an edge to W when
 it is encountered.

 The Walkup also determines the pertinent child bicomps that should be
 set up as a result of the need to embed edge (I, W). It does this by
 recording the pertinent child biconnected components of all cut
 vertices between W and the child of I that is a descendant of W.
 Note that it stops the traversal if it finds a visited flag set to I,
 which indicates that a prior walkup call in step I has already done
 the work.

 Zig and Zag are so named because one goes around one side of a
 bicomp and the other goes around the other side, yet we have
 as yet no notion of orientation for the bicomp.
 The edge J from vertex I gestures to an adjacent descendant vertex W
 (possibly in some other bicomp).  Zig and Zag start out at W.
 They go around alternate sides of the bicomp until its root is found.
 Recall that the root vertex is just a copy in region N to 2N-1.
 We want to hop from the root copy to the parent copy of the vertex
 in order to record which bicomp we just came from and also to continue
 the walk-up to vertex I.
 If the parent copy actually is I, then the walk-up is done.
 ********************************************************************/

void _WalkUp(graphP theGraph, int I, int J)
{
int  Zig, Zag, ZigPrevLink, ZagPrevLink;
int  N, R, ParentCopy, nextVertex, W;
int  RootID_DFSChild, BicompList;

     W = theGraph->G[J].v;
     theGraph->V[W].adjacentTo = J;

     /* Shorthand for N, due to frequent use */

     N = theGraph->N;

     /* Start at the vertex W and walk around the both sides of the external face
        of a bicomp until we get back to vertex I. */

     Zig = Zag = W;
     ZigPrevLink = 1;
     ZagPrevLink = 0;

     while (Zig != I)
     {
        /* A previous walk-up may have been this way already */

        if (theGraph->G[Zig].visited == I) break;
        if (theGraph->G[Zag].visited == I) break;

        /* Mark the current vertices as visited during the embedding of vertex I. */

        theGraph->G[Zig].visited = I;
        theGraph->G[Zag].visited = I;

        /* Determine whether either Zig or Zag has landed on a bicomp root */

        if (Zig >= N) R = Zig;
        else if (Zag >= N) R = Zag;
        else R = NIL;

        // If we have a bicomp root, then we want to hop up to the parent copy and
        // record a pertinent child bicomp.
        // Prepends if the bicomp is internally active, appends if externally active.

        if (R != NIL)
        {
            // The endpoints of a bicomp's "root edge" are the bicomp root R and a
            // DFS child of the parent copy of the bicomp root R.
            // The GraphNode location of the root vertices is in the range N to 2N-1
            // at the offset indicated by the associated DFS child.  So, the location
            // of the root vertex R, less N, is the location of the DFS child and also
            // a convenient identifier for the bicomp root.
            RootID_DFSChild = R - N;

            // It is extra unnecessary work to record pertinent bicomps of I
            if ((ParentCopy = theGraph->V[RootID_DFSChild].DFSParent) != I)
            {
                 // Get the BicompList of the parent copy vertex.
                 BicompList = theGraph->V[ParentCopy].pertinentBicompList;

                 /* Put the new root vertex in the BicompList.  It is prepended if internally
                    active and appended if externally active so that all internally
                    active bicomps are processed before any externally active bicomps
                    by virtue of storage.

                    NOTE: The activity status of a bicomp is computed using the lowpoint of
                            the DFS child in the bicomp's root edge because we want to know
                            whether the DFS child or any of its descendants are joined by a
                            back edge to ancestors of I. If so, then the bicomp rooted
                            at RootVertex must contain an externally active vertex so the
                            bicomp must be kept on the external face. */

                 if (theGraph->V[RootID_DFSChild].Lowpoint < I)
                      BicompList = LCAppend(theGraph->BicompLists, BicompList, RootID_DFSChild);
                 else BicompList = LCPrepend(theGraph->BicompLists, BicompList, RootID_DFSChild);

                 /* The head node of the parent copy vertex's bicomp list may have changed, so
                    we assign the head of the modified list as the vertex's pertinent
                    bicomp list */

                 theGraph->V[ParentCopy].pertinentBicompList = BicompList;
            }

            Zig = Zag = ParentCopy;
            ZigPrevLink = 1;
            ZagPrevLink = 0;
        }

        /* If we did not encounter a bicomp root, then we continue traversing the
            external face in both directions. */

        else
        {
            nextVertex = theGraph->extFace[Zig].link[1^ZigPrevLink];
            ZigPrevLink = theGraph->extFace[nextVertex].link[0] == Zig ? 0 : 1;
            Zig = nextVertex;

            nextVertex = theGraph->extFace[Zag].link[1^ZagPrevLink];
            ZagPrevLink = theGraph->extFace[nextVertex].link[0] == Zag ? 0 : 1;
            Zag = nextVertex;
        }
     }
}

/********************************************************************
 _HandleInactiveVertex()
 ********************************************************************/

int  _HandleInactiveVertex(graphP theGraph, int BicompRoot, int *pW, int *pWPrevLink)
{
     int X = theGraph->extFace[*pW].link[1^*pWPrevLink];
     *pWPrevLink = theGraph->extFace[X].link[0] == *pW ? 0 : 1;
     *pW = X;

     return OK;
}

/********************************************************************
 _GetPertinentChildBicomp()
 Returns the root of a pertinent child bicomp for the given vertex.
 Note: internally active roots are prepended by _Walkup()
 ********************************************************************/

#define _GetPertinentChildBicomp(theGraph, W) \
        (theGraph->V[W].pertinentBicompList==NIL \
         ? NIL \
         : theGraph->V[W].pertinentBicompList + theGraph->N)

/********************************************************************
 _WalkDown()
 Consider a circular shape with small circles and squares along its perimeter.
 The small circle at the top the root vertex of the bicomp.  The other small
 circles represent internally active vertices, and the squares represent
 externally active vertices.  The root vertex is a root copy of I, the
 vertex currently being processed.

 The Walkup previously marked all vertices adjacent to I by setting their
 adjacentTo flags.  Basically, we want to walkdown both the link[0] and
 then the link[1] sides of the bicomp rooted at RootVertex, embedding edges
 between it and descendants of I with the adjacentTo flag set.  It is sometimes
 necessary to hop to child biconnected components in order to reach the desired
 vertices and, in such cases, the biconnected components are merged together
 such that adding the back edge forms a new proper face in the biconnected
 component rooted at RootVertex (which, again, is a root copy of I).

 The outer loop performs both walks, unless the first walk got all the way
 around to RootVertex (only happens when bicomp contains no external activity,
 such as when processing the last vertex), or when non-planarity is
 discovered (in a pertinent child bicomp such that the stack is non-empty).

 For the inner loop, each iteration visits a vertex W.  If W is adjacentTo I,
 we call MergeBicomps to merge the biconnected components whose cut vertices
 have been collecting in theStack.  Then, we add the back edge (RootVertex, W)
 and clear the adjacentTo flag in W.

 Next, we check whether W has a pertinent child bicomp.  If so, then we figure
 out which path down from the root of the child bicomp leads to the next vertex
 to be visited, and we push onto the stack information on the cut vertex and
 the paths used to enter into it and exit from it.  Alternately, if W
 had no pertinent child bicomps, then we check to see if it is inactive.
 If so, we find the next vertex along the external face, then short-circuit
 its inactive predecessor (under certain conditions).  Finally, if W is not
 inactive, but it has no pertinent child bicomps, then we already know its
 adjacentTo flag is clear so both criteria for internal activity also fail.
 Therefore, W must be a stopping vertex.

 A stopping vertex X is an externally active vertex that has no pertinent
 child bicomps and no unembedded back edge to the current vertex I.
 The inner loop of Walkdown stops walking when it reaches a stopping vertex X
 because if it were to proceed beyond X and embed a back edge, then X would be
 surrounded by the bounding cycle of the bicomp.  This is clearly incorrect
 because X has a path leading from it to an ancestor of I (which is why it's
 externally active), and this path would have to cross the bounding cycle.

 After the loop, if the stack is non-empty, then the Walkdown halted because
 it could not proceed down a pertinent child biconnected component along either
 path from its root, which is easily shown to be evidence of a K_3,3, so
 we break the outer loop.  The caller performs further tests to determine
 whether Walkdown has embedded all back edges.  If the caller does not embed
 all back edges to descendants of the root vertex after walking both RootSide
 0 then 1 in all bicomps containing a root copy of I, then the caller can
 conclude that the input graph is non-planar.

  Returns OK if all possible edges were embedded, NONEMBEDDABLE if less
          than all possible edges were embedded, and NOTOK for an internal
          code failure
 ********************************************************************/

int  _WalkDown(graphP theGraph, int I, int RootVertex)
{
int  RetVal, W, WPrevLink, R, Rout, X, XPrevLink, Y, YPrevLink, RootSide, RootEdgeChild;

     RootEdgeChild = RootVertex - theGraph->N;

     sp_ClearStack(theGraph->theStack);

     for (RootSide = 0; RootSide < 2; RootSide++)
     {
         W = theGraph->extFace[RootVertex].link[RootSide];

         // The edge record in W that leads back to the root vertex
         // is indicated by link[1^RootSide] in W because only W
         // is in the bicomp with the root vertex.  When tree edges
         // are first embedded, it is done so that W has the same
         // orientation as the root vertex.
         WPrevLink = 1^RootSide;

         while (W != RootVertex)
         {
             /* If the vertex W is the descendant endpoint of an unembedded
                back edge to I, then ... */

             if (theGraph->V[W].adjacentTo != NIL)
             {
                /* Merge bicomps at cut vertices on theStack and add the back edge,
                    creating a new proper face. */

                if (sp_NonEmpty(theGraph->theStack))
                {
                    if ((RetVal = theGraph->functions.fpMergeBicomps(theGraph, I, RootVertex, W, WPrevLink)) != OK)
                        return RetVal;
                }
                theGraph->functions.fpEmbedBackEdgeToDescendant(theGraph, RootSide, RootVertex, W, WPrevLink);

                /* Clear W's AdjacentTo flag so we don't add another edge to W if
                    this invocation of Walkdown visits W again later (and more
                    generally, so that no more back edges to W are added until
                    a future Walkup sets the flag to non-NIL again). */

                theGraph->V[W].adjacentTo = NIL;
             }

             /* If there is a pertinent child bicomp, then we need to push it onto the stack
                along with information about how we entered the cut vertex and how
                we exit the root copy to get to the next vertex. */

             if (theGraph->V[W].pertinentBicompList != NIL)
             {
                 sp_Push2(theGraph->theStack, W, WPrevLink);
                 R = _GetPertinentChildBicomp(theGraph, W);

                 /* Get next active vertices X and Y on ext. face paths emanating from R */

                 X = theGraph->extFace[R].link[0];
                 XPrevLink = theGraph->extFace[X].link[1]==R ? 1 : 0;
                 Y = theGraph->extFace[R].link[1];
                 YPrevLink = theGraph->extFace[Y].link[0]==R ? 0 : 1;

                 /* If this is a bicomp with only two ext. face vertices, then
                    it could be that the orientation of the non-root vertex
                    doesn't match the orientation of the root due to our relaxed
                    orientation method. */

                 if (X == Y && theGraph->extFace[X].inversionFlag)
                 {
                     XPrevLink = 0;
                     YPrevLink = 1;
                 }

                 /* Now we implement the Walkdown's simple path selection rules!
                    If either X or Y is internally active (pertinent but not
                    externally active), then we pick it first.  Otherwise,
                    we choose a pertinent vertex. If neither are pertinent,
                    then we pick a vertex since the next iteration of the
                    loop will terminate on that vertex with a non-empty stack. */

                 if (_VertexActiveStatus(theGraph, X, I) == VAS_INTERNAL)
                      W = X;
                 else if (_VertexActiveStatus(theGraph, Y, I) == VAS_INTERNAL)
                      W = Y;
                 else if (PERTINENT(theGraph, X))
                      W = X;
                 else W = Y;

                 WPrevLink = W == X ? XPrevLink : YPrevLink;

                 Rout = W == X ? 0 : 1;
                 sp_Push2(theGraph->theStack, R, Rout);
             }

             /* Skip inactive vertices, which will be short-circuited
                later by our fast external face linking method (once
                upon a time, we added false edges called short-circuit
                edges to eliminate inactive vertices, but the extFace
                links can do the same job and also give us the ability
                to more quickly test planarity without creating an embedding). */

             else if (_VertexActiveStatus(theGraph, W, I) == VAS_INACTIVE)
             {
                 if (theGraph->functions.fpHandleInactiveVertex(theGraph, RootVertex, &W, &WPrevLink) != OK)
                     return NOTOK;
             }

             /* At this point, we know that W is not inactive, but its adjacentTo flag
                is clear, and it has no pertinent child bicomps.  Therefore, it
                is an externally active stopping vertex. */

             else break;
         }

         /* If the stack is non-empty, then we had a non-planarity condition,
            so we stop. */

         if (sp_NonEmpty(theGraph->theStack))
             return NONEMBEDDABLE;

         /* We short-circuit the external face of the bicomp by hooking the root
            to the terminating externally active vertex so that inactive vertices
            are not visited in future iterations.  This setting obviates the need
            for those short-circuit edges mentioned above.

            NOTE: We skip the step if the stack is non-empty since in that case
                    we did not actually merge the bicomps necessary to put
                    W and RootVertex into the same bicomp. */

         theGraph->extFace[RootVertex].link[RootSide] = W;
         theGraph->extFace[W].link[WPrevLink] = RootVertex;

         /* If the bicomp is reduced to having only two external face vertices
             (the root and W), then we need to record whether the orientation
             of W is inverted relative to the root.  This is used later when a
             future Walkdown descends to and merges the bicomp containing W.
             Going from the root to W, we only get the correct WPrevLink if
             we know whether or not W is inverted.
             NOTE: Prior code based on short-circuit edges did not have this problem
                 because the root and W would be joined by two separate short-circuit
                 edges, so G[W].link[0] != G[W].link[1].
             NOTE: We clear the flag because it may have been set in W if W
                 previously became part of a bicomp with only two ext. face
                 vertices, but then was flipped and merged into a larger bicomp
                 that is now again becoming a bicomp with only two ext. face vertices. */

         if (theGraph->extFace[W].link[0] == theGraph->extFace[W].link[1] &&
             WPrevLink == RootSide)
              theGraph->extFace[W].inversionFlag = 1;
         else theGraph->extFace[W].inversionFlag = 0;

         /* If we got back around to the root, then all edges
            are embedded, so we stop. */

         if (W == RootVertex)
             break;
     }

     return OK;
}


/********************************************************************
 gp_Embed()

  First, a DFS tree is created in the graph (if not already done).
  Then, the graph is sorted by DFI.

  Either a planar embedding is created in theGraph, or a Kuratowski
  subgraph is isolated.  Either way, theGraph remains sorted by DFI
  since that is the most common desired result.  The original vertex
  numbers are available in the 'v' members of the vertex graph nodes.
  Moreover, gp_SortVertices() can be invoked to put the vertices in
  the order of the input graph, at which point the 'v' members of the
  vertex graph nodes will contain the vertex DFIs.

 return OK if the embedding was successfully created or no subgraph
            homeomorphic to a topological obstruction was found.

        NOTOK on internal failure

        NONEMBEDDABLE if the embedding couldn't be created due to
                the existence of a subgraph homeomorphic to a
                topological obstruction.

  For core planarity, OK is returned when theGraph contains a planar
  embedding of the input graph, and NONEMBEDDABLE is returned when a
  subgraph homeomorphic to K5 or K3,3 has been isolated in theGraph.

  Extension modules can overload functions used by gp_Embed to achieve
  alternate algorithms.  In those cases, the return results are
  similar.  For example, a K3,3 search algorithm would return
  NONEMBEDDABLE if it finds the K3,3 obstruction, and OK if the graph
  is planar or only contains K5 homeomorphs.  Similarly, an
  outerplanarity module can return OK for an outerplanar embedding or
  NONEMBEDDABLE when a subgraph homeomorphic to K2,3 or K4 has been
  isolated.

  The algorithm extension for gp_Embed() is encoded in the embedFlags,
  and the details of the return value can be found in the extension
  module that defines the embedding flag.

 ********************************************************************/

int gp_Embed(graphP theGraph, int embedFlags)
{
int N, I, J, child;
int RetVal = OK;

    /* Basic parameter checks */

    if (theGraph==NULL)
    	return NOTOK;

    /* A little shorthand for the size of the graph */

    N = theGraph->N;

    /* Preprocessing */

    theGraph->embedFlags = embedFlags;

    if (gp_CreateDFSTree(theGraph) != OK)
        return NOTOK;

    if (!(theGraph->internalFlags & FLAGS_SORTEDBYDFI))
        if (gp_SortVertices(theGraph) != OK)
            return NOTOK;

    gp_LowpointAndLeastAncestor(theGraph);

    _CreateSortedSeparatedDFSChildLists(theGraph);

    if (theGraph->functions.fpCreateFwdArcLists(theGraph) != OK)
        return NOTOK;

    theGraph->functions.fpCreateDFSTreeEmbedding(theGraph);

    /* In reverse DFI order, process each vertex by embedding its
         the 'back edges' from the vertex to its DFS descendants. */

    for (I = 0; I < theGraph->edgeOffset; I++)
        theGraph->G[I].visited = N;

    for (I = theGraph->N-1; I >= 0; I--)
    {
          RetVal = OK;

          /* Do the Walkup for each cycle edge from I to a DFS descendant W. */

          J = theGraph->V[I].fwdArcList;
          while (J != NIL)
          {
              _WalkUp(theGraph, I, J);

              J = theGraph->G[J].link[0];
              if (J == theGraph->V[I].fwdArcList)
                  J = NIL;
          }

          /* For each DFS child C of the current vertex with a pertinent
                child bicomp, do a Walkdown on each side of the bicomp rooted
                by tree edge (R, C), where R is a root copy of the current
                vertex stored at C+N and uniquely associated with the bicomp
                containing C. (NOTE: if C has no pertinent child bicomps, then
                there are no cycle edges from I to descendants of C). */

          child = theGraph->V[I].separatedDFSChildList;
          while (child != NIL)
          {
              if (theGraph->V[child].pertinentBicompList != NIL)
              {
                  // _Walkdown returns OK even if it couldn't embed all
                  // back edges from I to the subtree rooted by child
                  // It only returns NONEMBEDDABLE when there is a
                  // non-empty stack of bicomps, the topmost of which
                  // is blocked up by stopping vertices along both
                  // external face paths emanating from the bicomp root
                  if ((RetVal = _WalkDown(theGraph, I, child + N)) != OK)
                  {
                      if (RetVal == NOTOK)
                           return NOTOK;
                      else break;
                  }
              }
              child = LCGetNext(theGraph->DFSChildLists,
                                theGraph->V[I].separatedDFSChildList, child);
          }

          /* If all Walkdown calls succeed, but they don't embed all of the
                forward edges, then the graph is non-planar. */

          if (theGraph->V[I].fwdArcList != NIL)
          {
              RetVal = theGraph->functions.fpEmbedIterationPostprocess(theGraph, I);
              if (RetVal != OK)
                  break;
          }
    }

    /* Postprocessing to orient the embedding and merge any remaining
       separated bicomps, or to isolate a Kuratowski subgraph */

    return theGraph->functions.fpEmbedPostprocess(theGraph, I, RetVal);
}

/********************************************************************
 _EmbedIterationPostprocess()

  At the end of each embedding iteration, this postprocess function
  decides whether to proceed to the next vertex.
  If some of the cycle edges from I to its descendants were not
  embedded, then the forward arc list of I will be non-empty.

  We return NONEMBEDDABLE to cause iteration to stop because the
  graph is non-planar if any edges could not be embedded.
  Otherwise, if the forward arc list is empty, then all cycle
  edges from I to its descendants were embedded, so we return OK
  so that the iteration will proceed.

  To stop iteration with an internal error, return NOTOK

  Extensions may overload this function and decide to proceed with or
  halt embedding iteration for application-specific reasons.  For
  example, a search for K3,3 homeomorphs could reduce a K5 homeomorph
  to something that can be ignored, and then continue the planarity
  algorithm in hopes of finding whether there is a K3,3 obstruction
  elsewhere in the graph.
 ********************************************************************/

int  _EmbedIterationPostprocess(graphP theGraph, int I)
{
     return NONEMBEDDABLE;
}

/********************************************************************
 _EmbedPostprocess()

 After the loop that embeds the cycle edges from each vertex to its
 DFS descendants, this method is invoked to postprocess the graph.
 If the graph is planar, then a consistent orientation is imposed
 on the vertices of the embedding, and any remaining separated
 biconnected components are joined together.
 If the graph is non-planar, then a subgraph homeomorphic to K5
 or K3,3 is isolated.
 Extensions may override this function to provide alternate
 behavior.

  @param theGraph - the graph ready for postprocessing
  @param I - the last vertex processed by the edge embedding loop
  @param edgeEmbeddingResult -
         OK if all edge embedding iterations returned OK
         NONEMBEDDABLE if an embedding iteration failed to embed
             all edges for a vertex

  @return NOTOK on internal failure
          NONEMBEDDABLE if a subgraph homeomorphic to a topological
              obstruction is isolated in the graph
          OK otherwise (for example if the graph contains a
             planar embedding or if a desired topological obstruction
             was not found)

 *****************************************************************/

int  _EmbedPostprocess(graphP theGraph, int I, int edgeEmbeddingResult)
{
int  RetVal = edgeEmbeddingResult;

    /* If an embedding was found, then post-process the embedding structure
        to eliminate root copies and give a consistent orientation to all vertices. */

    if (edgeEmbeddingResult == OK)
    {
        _OrientVerticesInEmbedding(theGraph);
        _JoinBicomps(theGraph);
    }

    /* If the graph was found to be unembeddable, then we want to isolate an
        obstruction.  But, if a search flag was set, then we have already
        found a subgraph with the desired structure, so no further work is done. */

    else if (edgeEmbeddingResult == NONEMBEDDABLE)
    {
        if (theGraph->embedFlags == EMBEDFLAGS_PLANAR)
        {
            if (_IsolateKuratowskiSubgraph(theGraph, I) != OK)
                RetVal = NOTOK;
        }
        else if (theGraph->embedFlags == EMBEDFLAGS_OUTERPLANAR)
        {
            if (_IsolateOuterplanarObstruction(theGraph, I) != OK)
                RetVal = NOTOK;
        }
    }

    return RetVal;
}

/********************************************************************
 _OrientVerticesInEmbedding()

 Each vertex will then have an orientation, either clockwise or
 counterclockwise.  All vertices in each bicomp need to have the
 same orientation.
 ********************************************************************/

void _OrientVerticesInEmbedding(graphP theGraph)
{
int  R, edgeOffset = theGraph->edgeOffset;

     sp_ClearStack(theGraph->theStack);

/* Run the array of root copy vertices.  For each that is not defunct
        (i.e. has not been merged during embed), we orient the vertices
        in the bicomp for which it is the root vertex. */

     for (R = theGraph->N; R < edgeOffset; R++)
          if (theGraph->G[R].link[0] != NIL)
              _OrientVerticesInBicomp(theGraph, R, 0);
}

/********************************************************************
 _OrientVerticesInBicomp()
  As a result of the work done so far, the edges around each vertex have
 been put in order, but the orientation may be counterclockwise or
 clockwise for different vertices within the same bicomp.
 We need to reverse the orientations of those vertices that are not
 oriented the same way as the root of the bicomp.

 During embedding, a bicomp with root edge (v', c) may need to be flipped.
 We do this by inverting the root copy v' and implicitly inverting the
 orientation of the vertices in the subtree rooted by c by assigning -1
 to the sign of the DFSCHILD edge record leading to c.

 We now use these signs to help propagate a consistent vertex orientation
 throughout all vertices that have been merged into the given bicomp.
 The bicomp root contains the orientation to be imposed on all parent
 copy vertices.  We perform a standard depth first search to visit each
 vertex.  A vertex must be inverted if the product of the edge signs
 along the tree edges between the bicomp root and the vertex is -1.

 Finally, the PreserveSigns flag, if set, performs the inversions
 but does not change any of the edge signs.  This allows a second
 invocation of this function to restore the state of the bicomp
 as it was before the first call.
 ********************************************************************/

void _OrientVerticesInBicomp(graphP theGraph, int BicompRoot, int PreserveSigns)
{
int  V, J, invertedFlag;

     sp_ClearStack(theGraph->theStack);
     sp_Push2(theGraph->theStack, BicompRoot, 0);

     while (sp_NonEmpty(theGraph->theStack))
     {
         /* Pop a vertex to orient */
         sp_Pop2(theGraph->theStack, V, invertedFlag);

         /* Invert the vertex if the inverted flag is set */
         if (invertedFlag)
             _InvertVertex(theGraph, V);

         /* Push the vertex's DFS children that are in the bicomp */
         J = theGraph->G[V].link[0];
         while (J >= theGraph->edgeOffset)
         {
             if (theGraph->G[J].type == EDGE_DFSCHILD)
             {
                 sp_Push2(theGraph->theStack, theGraph->G[J].v,
                		  invertedFlag ^ GET_EDGEFLAG_INVERTED(theGraph, J));

                 if (!PreserveSigns)
                	 CLEAR_EDGEFLAG_INVERTED(theGraph, J);
             }

             J = theGraph->G[J].link[0];
         }
     }
}

/********************************************************************
 _JoinBicomps()
 The embedding algorithm works by only joining bicomps once the result
 forms a larger bicomp.  However, if the original graph was separable
 or disconnected, then the result of the embed function will be a
 graph that contains each bicomp as a distinct entity.  The root of
 each bicomp will be in the region N to 2N-1.  This function merges
 the bicomps into one connected graph.
 ********************************************************************/

int  _JoinBicomps(graphP theGraph)
{
int  R, N, edgeOffset=theGraph->edgeOffset;

     for (R=N=theGraph->N; R < edgeOffset; R++)
          if (theGraph->G[R].link[0] != NIL)
              _MergeVertex(theGraph, theGraph->V[R-N].DFSParent, 0, R);

     return OK;
}
