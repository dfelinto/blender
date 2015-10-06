
BMesh Operators (bmesh.ops)
===========================

.. module:: bmesh.ops

This module gives access to low level bmesh operations.

Most operators take input and return output, they can be chained together
to perform useful operations.

.. note::

   This API us new in 2.65 and not yet well tested.


Operator Example
++++++++++++++++
This script shows how operators can be used to model a link of a chain.

.. literalinclude:: ../examples/bmesh.ops.1.py
.. function:: smooth_vert(bm, verts, factor, mirror_clip_x, mirror_clip_y, mirror_clip_z, clip_dist, use_axis_x, use_axis_y, use_axis_z)

   Vertex Smooth.

   Smooths vertices by using a basic vertex averaging scheme.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg factor: smoothing factor
   :type factor: float
   :arg mirror_clip_x: set vertices close to the x axis before the operation to 0
   :type mirror_clip_x: bool
   :arg mirror_clip_y: set vertices close to the y axis before the operation to 0
   :type mirror_clip_y: bool
   :arg mirror_clip_z: set vertices close to the z axis before the operation to 0
   :type mirror_clip_z: bool
   :arg clip_dist: clipping threshold for the above three slots
   :type clip_dist: float
   :arg use_axis_x: smooth vertices along X axis
   :type use_axis_x: bool
   :arg use_axis_y: smooth vertices along Y axis
   :type use_axis_y: bool
   :arg use_axis_z: smooth vertices along Z axis
   :type use_axis_z: bool


.. function:: smooth_laplacian_vert(bm, verts, lambda_factor, lambda_border, use_x, use_y, use_z, preserve_volume)

   Vertext Smooth Laplacian.

   Smooths vertices by using Laplacian smoothing propose by.
   Desbrun, et al. Implicit Fairing of Irregular Meshes using Diffusion and Curvature Flow.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg lambda_factor: lambda param
   :type lambda_factor: float
   :arg lambda_border: lambda param in border
   :type lambda_border: float
   :arg use_x: Smooth object along X axis
   :type use_x: bool
   :arg use_y: Smooth object along Y axis
   :type use_y: bool
   :arg use_z: Smooth object along Z axis
   :type use_z: bool
   :arg preserve_volume: Apply volume preservation after smooth
   :type preserve_volume: bool


.. function:: recalc_face_normals(bm, faces)

   Right-Hand Faces.

   Computes an "outside" normal for the specified input faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: Undocumented.
   :type faces: list of (:class:`bmesh.types.BMFace`)


.. function:: planar_faces(bm, faces, iterations, factor)

   Planar Faces.

   Iteratively flatten faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input geometry.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg iterations: Undocumented.
   :type iterations: int
   :arg factor: planar factor
   :type factor: float
   :return:

      - ``geom``: output slot, computed boundary geometry.

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: region_extend(bm, geom, use_contract, use_faces, use_face_step)

   Region Extend.

   used to implement the select more/less tools.
   this puts some geometry surrounding regions of
   geometry in geom into geom.out.

   if use_faces is 0 then geom.out spits out verts and edges,
   otherwise it spits out faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: input geometry
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg use_contract: find boundary inside the regions, not outside.
   :type use_contract: bool
   :arg use_faces: extend from faces instead of edges
   :type use_faces: bool
   :arg use_face_step: step over connected faces
   :type use_face_step: bool
   :return:

      - ``geom``: output slot, computed boundary geometry.

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: rotate_edges(bm, edges, use_ccw)

   Edge Rotate.

   Rotates edges topologically.  Also known as "spin edge" to some people.
   Simple example: ``[/] becomes [|] then [\]``.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_ccw: rotate edge counter-clockwise if true, otherwise clockwise
   :type use_ccw: bool
   :return:

      - ``edges``: newly spun edges

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: reverse_faces(bm, faces)

   Reverse Faces.

   Reverses the winding (vertex order) of faces.
   This has the effect of flipping the normal.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)


.. function:: bisect_edges(bm, edges, cuts, edge_percents)

   Edge Bisect.

   Splits input edges (but doesn't do anything else).
   This creates a 2-valence vert.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg cuts: number of cuts
   :type cuts: int
   :arg edge_percents: Undocumented.
   :type edge_percents: dict mapping vert/edge/face types to float
   :return:

      - ``geom_split``: newly created vertices and edges

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: mirror(bm, geom, matrix, merge_dist, axis, mirror_u, mirror_v)

   Mirror.

   Mirrors geometry along an axis.  The resulting geometry is welded on using
   merge_dist.  Pairs of original/mirrored vertices are welded using the merge_dist
   parameter (which defines the minimum distance for welding to happen).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: input geometry
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg matrix: matrix defining the mirror transformation
   :type matrix: :class:`mathutils.Matrix`
   :arg merge_dist: maximum distance for merging.  does no merging if 0.
   :type merge_dist: float
   :arg axis: the axis to use, 0, 1, or 2 for x, y, z
   :type axis: int
   :arg mirror_u: mirror UVs across the u axis
   :type mirror_u: bool
   :arg mirror_v: mirror UVs across the v axis
   :type mirror_v: bool
   :return:

      - ``geom``: output geometry, mirrored

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: find_doubles(bm, verts, keep_verts, dist)

   Find Doubles.

   Takes input verts and find vertices they should weld to.
   Outputs a mapping slot suitable for use with the weld verts bmop.

   If keep_verts is used, vertices outside that set can only be merged
   with vertices in that set.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg keep_verts: list of verts to keep
   :type keep_verts: list of (:class:`bmesh.types.BMVert`)
   :arg dist: minimum distance
   :type dist: float
   :return:

      - ``targetmap``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`

   :rtype: dict with string keys


.. function:: remove_doubles(bm, verts, dist)

   Remove Doubles.

   Finds groups of vertices closer then dist and merges them together,
   using the weld verts bmop.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input verts
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg dist: minimum distance
   :type dist: float


.. function:: automerge(bm, verts, dist)

   Auto Merge.

   Finds groups of vertices closer then **dist** and merges them together,
   using the weld verts bmop.  The merges must go from a vert not in
   **verts** to one in **verts**.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input verts
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg dist: minimum distance
   :type dist: float


.. function:: collapse(bm, edges, uvs)

   Collapse Connected.

   Collapses connected vertices

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg uvs: also collapse UVs and such
   :type uvs: bool


.. function:: pointmerge_facedata(bm, verts, vert_snap)

   Face-Data Point Merge.

   Merge uv/vcols at a specific vertex.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg vert_snap: snap vertex
   :type vert_snap: :class:`bmesh.types.BMVert`


.. function:: average_vert_facedata(bm, verts)

   Average Vertices Facevert Data.

   Merge uv/vcols associated with the input vertices at
   the bounding box center. (I know, it's not averaging but
   the vert_snap_to_bb_center is just too long).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)


.. function:: pointmerge(bm, verts, merge_co)

   Point Merge.

   Merge verts together at a point.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg merge_co: Undocumented.
   :type merge_co: :class:`mathutils.Vector` or any sequence of 3 floats


.. function:: collapse_uvs(bm, edges)

   Collapse Connected UV's.

   Collapses connected UV vertices.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)


.. function:: weld_verts(bm, targetmap)

   Weld Verts.

   Welds verts together (kind-of like remove doubles, merge, etc, all of which
   use or will use this bmop).  You pass in mappings from vertices to the vertices
   they weld with.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg targetmap: Undocumented.
   :type targetmap: dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`


.. function:: create_vert(bm, co)

   Make Vertex.

   Creates a single vertex; this bmop was necessary
   for click-create-vertex.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg co: the coordinate of the new vert
   :type co: :class:`mathutils.Vector` or any sequence of 3 floats
   :return:

      - ``vert``: the new vert

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: join_triangles(bm, faces, cmp_seam, cmp_sharp, cmp_uvs, cmp_vcols, cmp_materials, angle_face_threshold, angle_shape_threshold)

   Join Triangles.

   Tries to intelligently join triangles according
   to angle threshold and delimiters.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input geometry.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg cmp_seam: Undocumented.
   :type cmp_seam: bool
   :arg cmp_sharp: Undocumented.
   :type cmp_sharp: bool
   :arg cmp_uvs: Undocumented.
   :type cmp_uvs: bool
   :arg cmp_vcols: Undocumented.
   :type cmp_vcols: bool
   :arg cmp_materials: Undocumented.
   :type cmp_materials: bool
   :arg angle_face_threshold: Undocumented.
   :type angle_face_threshold: float
   :arg angle_shape_threshold: Undocumented.
   :type angle_shape_threshold: float
   :return:

      - ``faces``: joined faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: contextual_create(bm, geom, mat_nr, use_smooth)

   Contextual Create.

   This is basically F-key, it creates
   new faces from vertices, makes stuff from edge nets,
   makes wire edges, etc.  It also dissolves faces.

   Three verts become a triangle, four become a quad.  Two
   become a wire edge.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: input geometry.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg mat_nr: material to use
   :type mat_nr: int
   :arg use_smooth: smooth to use
   :type use_smooth: bool
   :return:

      - ``faces``: newly-made face(s)

        **type** list of (:class:`bmesh.types.BMFace`)
      - ``edges``: newly-made edge(s)

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: bridge_loops(bm, edges, use_pairs, use_cyclic, use_merge, merge_factor, twist_offset)

   Bridge edge loops with faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_pairs: Undocumented.
   :type use_pairs: bool
   :arg use_cyclic: Undocumented.
   :type use_cyclic: bool
   :arg use_merge: Undocumented.
   :type use_merge: bool
   :arg merge_factor: Undocumented.
   :type merge_factor: float
   :arg twist_offset: Undocumented.
   :type twist_offset: int
   :return:

      - ``faces``: new faces

        **type** list of (:class:`bmesh.types.BMFace`)
      - ``edges``: new edges

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: grid_fill(bm, edges, mat_nr, use_smooth, use_interp_simple)

   Grid Fill.

   Create faces defined by 2 disconnected edge loops (which share edges).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg mat_nr: material to use
   :type mat_nr: int
   :arg use_smooth: smooth state to use
   :type use_smooth: bool
   :arg use_interp_simple: use simple interpolation
   :type use_interp_simple: bool
   :return:

      - ``faces``: new faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: holes_fill(bm, edges, sides)

   Fill Holes.

   Fill boundary edges with faces, copying surrounding customdata.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg sides: number of face sides to fill
   :type sides: int
   :return:

      - ``faces``: new faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: face_attribute_fill(bm, faces, use_normals, use_data)

   Face Attribute Fill.

   Fill in faces with data from adjacent faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg use_normals: copy face winding
   :type use_normals: bool
   :arg use_data: copy face data
   :type use_data: bool
   :return:

      - ``faces_fail``: faces that could not be handled

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: edgeloop_fill(bm, edges, mat_nr, use_smooth)

   Edge Loop Fill.

   Create faces defined by one or more non overlapping edge loops.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg mat_nr: material to use
   :type mat_nr: int
   :arg use_smooth: smooth state to use
   :type use_smooth: bool
   :return:

      - ``faces``: new faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: edgenet_fill(bm, edges, mat_nr, use_smooth, sides)

   Edge Net Fill.

   Create faces defined by enclosed edges.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg mat_nr: material to use
   :type mat_nr: int
   :arg use_smooth: smooth state to use
   :type use_smooth: bool
   :arg sides: number of sides
   :type sides: int
   :return:

      - ``faces``: new faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: edgenet_prepare(bm, edges)

   Edgenet Prepare.

   Identifies several useful edge loop cases and modifies them so
   they'll become a face when edgenet_fill is called.  The cases covered are:

   - One single loop; an edge is added to connect the ends
   - Two loops; two edges are added to connect the endpoints (based on the
     shortest distance between each endpont).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :return:

      - ``edges``: new edges

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: rotate(bm, cent, matrix, verts, space)

   Rotate.

   Rotate vertices around a center, using a 3x3 rotation matrix.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg cent: center of rotation
   :type cent: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg matrix: matrix defining rotation
   :type matrix: :class:`mathutils.Matrix`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg space: matrix to define the space (typically object matrix)
   :type space: :class:`mathutils.Matrix`


.. function:: translate(bm, vec, space, verts)

   Translate.

   Translate vertices by an offset.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg vec: translation offset
   :type vec: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg space: matrix to define the space (typically object matrix)
   :type space: :class:`mathutils.Matrix`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)


.. function:: scale(bm, vec, space, verts)

   Scale.

   Scales vertices by an offset.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg vec: scale factor
   :type vec: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg space: matrix to define the space (typically object matrix)
   :type space: :class:`mathutils.Matrix`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)


.. function:: transform(bm, matrix, space, verts)

   Transform.

   Transforms a set of vertices by a matrix.  Multiplies
   the vertex coordinates with the matrix.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg matrix: transform matrix
   :type matrix: :class:`mathutils.Matrix`
   :arg space: matrix to define the space (typically object matrix)
   :type space: :class:`mathutils.Matrix`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)


.. function:: object_load_bmesh(bm, scene, object)

   Object Load BMesh.

   Loads a bmesh into an object/mesh.  This is a "private"
   bmop.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg scene: Undocumented.
   :type scene: :class:`bpy.types.Scene`
   :arg object: Undocumented.
   :type object: :class:`bpy.types.Object`


.. function:: bmesh_to_mesh(bm, mesh, object, skip_tessface)

   BMesh to Mesh.

   Converts a bmesh to a Mesh.  This is reserved for exiting editmode.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg mesh: Undocumented.
   :type mesh: :class:`bpy.types.Mesh`
   :arg object: Undocumented.
   :type object: :class:`bpy.types.Object`
   :arg skip_tessface: don't calculate mfaces
   :type skip_tessface: bool


.. function:: mesh_to_bmesh(bm, mesh, object, use_shapekey)

   Mesh to BMesh.

   Load the contents of a mesh into the bmesh.  this bmop is private, it's
   reserved exclusively for entering editmode.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg mesh: Undocumented.
   :type mesh: :class:`bpy.types.Mesh`
   :arg object: Undocumented.
   :type object: :class:`bpy.types.Object`
   :arg use_shapekey: load active shapekey coordinates into verts
   :type use_shapekey: bool


.. function:: extrude_discrete_faces(bm, faces, use_select_history)

   Individual Face Extrude.

   Extrudes faces individually.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg use_select_history: pass to duplicate
   :type use_select_history: bool
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: extrude_edge_only(bm, edges, use_select_history)

   Extrude Only Edges.

   Extrudes Edges into faces, note that this is very simple, there's no fancy
   winged extrusion.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input vertices
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_select_history: pass to duplicate
   :type use_select_history: bool
   :return:

      - ``geom``: output geometry

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: extrude_vert_indiv(bm, verts, use_select_history)

   Individual Vertex Extrude.

   Extrudes wire edges from vertices.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg use_select_history: pass to duplicate
   :type use_select_history: bool
   :return:

      - ``edges``: output wire edges

        **type** list of (:class:`bmesh.types.BMEdge`)
      - ``verts``: output vertices

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: connect_verts(bm, verts, faces_exclude, check_degenerate)

   Connect Verts.

   Split faces by adding edges that connect **verts**.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: Undocumented.
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg faces_exclude: Undocumented.
   :type faces_exclude: list of (:class:`bmesh.types.BMFace`)
   :arg check_degenerate: prevent splits with overlaps & intersections
   :type check_degenerate: bool
   :return:

      - ``edges``: 

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: connect_verts_concave(bm, faces)

   Connect Verts to form Convex Faces.

   Ensures all faces are convex **faces**.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: Undocumented.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :return:

      - ``edges``: 

        **type** list of (:class:`bmesh.types.BMEdge`)
      - ``faces``: 

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: connect_verts_nonplanar(bm, angle_limit, faces)

   Connect Verts Across non Planer Faces.

   Split faces by connecting edges along non planer **faces**.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg angle_limit: total rotation angle (radians)
   :type angle_limit: float
   :arg faces: Undocumented.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :return:

      - ``edges``: 

        **type** list of (:class:`bmesh.types.BMEdge`)
      - ``faces``: 

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: connect_vert_pair(bm, verts, verts_exclude, faces_exclude)

   Connect Verts.

   Split faces by adding edges that connect **verts**.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: Undocumented.
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg verts_exclude: Undocumented.
   :type verts_exclude: list of (:class:`bmesh.types.BMVert`)
   :arg faces_exclude: Undocumented.
   :type faces_exclude: list of (:class:`bmesh.types.BMFace`)
   :return:

      - ``edges``: 

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: extrude_face_region(bm, geom, edges_exclude, use_keep_orig, use_select_history)

   Extrude Faces.

   Extrude operator (does not transform)

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: edges and faces
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg edges_exclude: Undocumented.
   :type edges_exclude: set of vert/edge/face type
   :arg use_keep_orig: keep original geometry
   :type use_keep_orig: bool
   :arg use_select_history: pass to duplicate
   :type use_select_history: bool
   :return:

      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: dissolve_verts(bm, verts, use_face_split, use_boundary_tear)

   Dissolve Verts.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: Undocumented.
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg use_face_split: Undocumented.
   :type use_face_split: bool
   :arg use_boundary_tear: Undocumented.
   :type use_boundary_tear: bool


.. function:: dissolve_edges(bm, edges, use_verts, use_face_split)

   Dissolve Edges.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: Undocumented.
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_verts: dissolve verts left between only 2 edges.
   :type use_verts: bool
   :arg use_face_split: Undocumented.
   :type use_face_split: bool
   :return:

      - ``region``: 

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: dissolve_faces(bm, faces, use_verts)

   Dissolve Faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: Undocumented.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg use_verts: dissolve verts left between only 2 edges.
   :type use_verts: bool
   :return:

      - ``region``: 

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: dissolve_limit(bm, angle_limit, use_dissolve_boundaries, verts, edges, delimit)

   Limited Dissolve.

   Dissolve planar faces and co-linear edges.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg angle_limit: total rotation angle (radians)
   :type angle_limit: float
   :arg use_dissolve_boundaries: Undocumented.
   :type use_dissolve_boundaries: bool
   :arg verts: Undocumented.
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg edges: Undocumented.
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg delimit: Undocumented.
   :type delimit: int
   :return:

      - ``region``: 

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: dissolve_degenerate(bm, dist, edges)

   Degenerate Dissolve.

   Dissolve edges with no length, faces with no area.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg dist: minimum distance to consider degenerate
   :type dist: float
   :arg edges: Undocumented.
   :type edges: list of (:class:`bmesh.types.BMEdge`)


.. function:: triangulate(bm, faces, quad_method, ngon_method)

   Triangulate.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: Undocumented.
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg quad_method: Undocumented.
   :type quad_method: int
   :arg ngon_method: Undocumented.
   :type ngon_method: int
   :return:

      - ``edges``: 

        **type** list of (:class:`bmesh.types.BMEdge`)
      - ``faces``: 

        **type** list of (:class:`bmesh.types.BMFace`)
      - ``face_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`

   :rtype: dict with string keys


.. function:: unsubdivide(bm, verts, iterations)

   Un-Subdivide.

   Reduce detail in geometry containing grids.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg iterations: Undocumented.
   :type iterations: int


.. function:: subdivide_edges(bm, edges, smooth, smooth_falloff, fractal, along_normal, cuts, seed, custom_patterns, edge_percents, quad_corner_type, use_grid_fill, use_single_edge, use_only_quads, use_sphere, use_smooth_even)

   Subdivide Edges.

   Advanced operator for subdividing edges
   with options for face patterns, smoothing and randomization.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: Undocumented.
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg smooth: Undocumented.
   :type smooth: float
   :arg smooth_falloff: SUBD_FALLOFF_ROOT and friends
   :type smooth_falloff: int
   :arg fractal: Undocumented.
   :type fractal: float
   :arg along_normal: Undocumented.
   :type along_normal: float
   :arg cuts: Undocumented.
   :type cuts: int
   :arg seed: Undocumented.
   :type seed: int
   :arg custom_patterns: uses custom pointers
   :type custom_patterns: dict mapping vert/edge/face types to unknown internal data, not compatible with python
   :arg edge_percents: Undocumented.
   :type edge_percents: dict mapping vert/edge/face types to float
   :arg quad_corner_type: quad corner type, see bmesh_operators.h
   :type quad_corner_type: int
   :arg use_grid_fill: fill in fully-selected faces with a grid
   :type use_grid_fill: bool
   :arg use_single_edge: tessellate the case of one edge selected in a quad or triangle
   :type use_single_edge: bool
   :arg use_only_quads: only subdivide quads (for loopcut)
   :type use_only_quads: bool
   :arg use_sphere: for making new primitives only
   :type use_sphere: bool
   :arg use_smooth_even: maintain even offset when smoothing
   :type use_smooth_even: bool
   :return:

      - ``geom_inner``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom_split``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom``: contains all output geometry

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: subdivide_edgering(bm, edges, interp_mode, smooth, cuts, profile_shape, profile_shape_factor)

   Subdivide Edge-Ring.

   Take an edge-ring, and subdivide with interpolation options.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input vertices
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg interp_mode: Undocumented.
   :type interp_mode: int
   :arg smooth: Undocumented.
   :type smooth: float
   :arg cuts: Undocumented.
   :type cuts: int
   :arg profile_shape: Undocumented.
   :type profile_shape: int
   :arg profile_shape_factor: Undocumented.
   :type profile_shape_factor: float
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: bisect_plane(bm, geom, dist, plane_co, plane_no, use_snap_center, clear_outer, clear_inner)

   Bisect Plane.

   Bisects the mesh by a plane (cut the mesh in half).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg dist: minimum distance when testing if a vert is exactly on the plane
   :type dist: float
   :arg plane_co: point on the plane
   :type plane_co: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg plane_no: direction of the plane
   :type plane_no: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg use_snap_center: snap axis aligned verts to the center
   :type use_snap_center: bool
   :arg clear_outer: when enabled. remove all geometry on the positive side of the plane
   :type clear_outer: bool
   :arg clear_inner: when enabled. remove all geometry on the negative side of the plane
   :type clear_inner: bool
   :return:

      - ``geom_cut``: output new geometry from the cut

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`)
      - ``geom``: input and output geometry (result of cut)

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: delete(bm, geom, context)

   Delete Geometry.

   Utility operator to delete geometry.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg context: enum DEL_VERTS ...
   :type context: int


.. function:: duplicate(bm, geom, dest, use_select_history)

   Duplicate Geometry.

   Utility operator to duplicate geometry,
   optionally into a destination mesh.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg dest: Undocumented.
   :type dest: :class:`bmesh.types.BMesh`
   :arg use_select_history: Undocumented.
   :type use_select_history: bool
   :return:

      - ``geom_orig``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``vert_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`
      - ``edge_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`
      - ``face_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`
      - ``boundary_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`
      - ``isovert_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`

   :rtype: dict with string keys


.. function:: split(bm, geom, dest, use_only_faces)

   Split Off Geometry.

   Disconnect geometry from adjacent edges and faces,
   optionally into a destination mesh.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg dest: Undocumented.
   :type dest: :class:`bmesh.types.BMesh`
   :arg use_only_faces: when enabled. don't duplicate loose verts/edges
   :type use_only_faces: bool
   :return:

      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``boundary_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`
      - ``isovert_map``: 

        **type** dict mapping vert/edge/face types to :class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`

   :rtype: dict with string keys


.. function:: spin(bm, geom, cent, axis, dvec, angle, space, steps, use_duplicate)

   Spin.

   Extrude or duplicate geometry a number of times,
   rotating and possibly translating after each step

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg cent: rotation center
   :type cent: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg axis: rotation axis
   :type axis: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg dvec: translation delta per step
   :type dvec: :class:`mathutils.Vector` or any sequence of 3 floats
   :arg angle: total rotation angle (radians)
   :type angle: float
   :arg space: matrix to define the space (typically object matrix)
   :type space: :class:`mathutils.Matrix`
   :arg steps: number of steps
   :type steps: int
   :arg use_duplicate: duplicate or extrude?
   :type use_duplicate: bool
   :return:

      - ``geom_last``: result of last step

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: similar_faces(bm, faces, type, thresh, compare)

   Similar Faces Search.

   Find similar faces (area/material/perimeter, ...).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg type: type of selection
   :type type: int
   :arg thresh: threshold of selection
   :type thresh: float
   :arg compare: comparison method
   :type compare: int
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: similar_edges(bm, edges, type, thresh, compare)

   Similar Edges Search.

    Find similar edges (length, direction, edge, seam, ...).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg type: type of selection
   :type type: int
   :arg thresh: threshold of selection
   :type thresh: float
   :arg compare: comparison method
   :type compare: int
   :return:

      - ``edges``: output edges

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: similar_verts(bm, verts, type, thresh, compare)

   Similar Verts Search.

   Find similar vertices (normal, face, vertex group, ...).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg verts: input vertices
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg type: type of selection
   :type type: int
   :arg thresh: threshold of selection
   :type thresh: float
   :arg compare: comparison method
   :type compare: int
   :return:

      - ``verts``: output vertices

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: rotate_uvs(bm, faces, use_ccw)

   UV Rotation.

   Cycle the loop UV's

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg use_ccw: rotate counter-clockwise if true, otherwise clockwise
   :type use_ccw: bool


.. function:: reverse_uvs(bm, faces)

   UV Reverse.

   Reverse the UV's

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)


.. function:: rotate_colors(bm, faces, use_ccw)

   Color Rotation.

   Cycle the loop colors

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg use_ccw: rotate counter-clockwise if true, otherwise clockwise
   :type use_ccw: bool


.. function:: reverse_colors(bm, faces)

   Color Reverse

   Reverse the loop colors.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)


.. function:: split_edges(bm, edges, verts, use_verts)

   Edge Split.

   Disconnects faces along input edges.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg verts: optional tag verts, use to have greater control of splits
   :type verts: list of (:class:`bmesh.types.BMVert`)
   :arg use_verts: use 'verts' for splitting, else just find verts to split from edges
   :type use_verts: bool
   :return:

      - ``edges``: old output disconnected edges

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: create_grid(bm, x_segments, y_segments, size, matrix)

   Create Grid.

   Creates a grid with a variable number of subdivisions

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg x_segments: number of x segments
   :type x_segments: int
   :arg y_segments: number of y segments
   :type y_segments: int
   :arg size: size of the grid
   :type size: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_uvsphere(bm, u_segments, v_segments, diameter, matrix)

   Create UV Sphere.

   Creates a grid with a variable number of subdivisions

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg u_segments: number of u segments
   :type u_segments: int
   :arg v_segments: number of v segment
   :type v_segments: int
   :arg diameter: diameter
   :type diameter: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_icosphere(bm, subdivisions, diameter, matrix)

   Create Ico-Sphere.

   Creates a grid with a variable number of subdivisions

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg subdivisions: how many times to recursively subdivide the sphere
   :type subdivisions: int
   :arg diameter: diameter
   :type diameter: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_monkey(bm, matrix)

   Create Suzanne.

   Creates a monkey (standard blender primitive).

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_cone(bm, cap_ends, cap_tris, segments, diameter1, diameter2, depth, matrix)

   Create Cone.

   Creates a cone with variable depth at both ends

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg cap_ends: whether or not to fill in the ends with faces
   :type cap_ends: bool
   :arg cap_tris: fill ends with triangles instead of ngons
   :type cap_tris: bool
   :arg segments: Undocumented.
   :type segments: int
   :arg diameter1: diameter of one end
   :type diameter1: float
   :arg diameter2: diameter of the opposite
   :type diameter2: float
   :arg depth: distance between ends
   :type depth: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_circle(bm, cap_ends, cap_tris, segments, diameter, matrix)

   Creates a Circle.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg cap_ends: whether or not to fill in the ends with faces
   :type cap_ends: bool
   :arg cap_tris: fill ends with triangles instead of ngons
   :type cap_tris: bool
   :arg segments: Undocumented.
   :type segments: int
   :arg diameter: diameter of one end
   :type diameter: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: create_cube(bm, size, matrix)

   Create Cube

   Creates a cube.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg size: size of the cube
   :type size: float
   :arg matrix: matrix to multiply the new geometry with
   :type matrix: :class:`mathutils.Matrix`
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)

   :rtype: dict with string keys


.. function:: bevel(bm, geom, offset, offset_type, segments, profile, vertex_only, clamp_overlap, material, loop_slide)

   Bevel.

   Bevels edges and vertices

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: input edges and vertices
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg offset: amount to offset beveled edge
   :type offset: float
   :arg offset_type: how to measure offset (enum)
   :type offset_type: int
   :arg segments: number of segments in bevel
   :type segments: int
   :arg profile: profile shape, 0->1 (.5=>round)
   :type profile: float
   :arg vertex_only: only bevel vertices, not edges
   :type vertex_only: bool
   :arg clamp_overlap: do not allow beveled edges/vertices to overlap each other
   :type clamp_overlap: bool
   :arg material: material for bevel faces, -1 means get from adjacent faces
   :type material: int
   :arg loop_slide: prefer to slide along edges to having even widths
   :type loop_slide: bool
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: beautify_fill(bm, faces, edges, use_restrict_tag, method)

   Beautify Fill.

   Rotate edges to create more evenly spaced triangles.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg edges: edges that can be flipped
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_restrict_tag: restrict edge rotation to mixed tagged vertices
   :type use_restrict_tag: bool
   :arg method: method to define what is beautiful
   :type method: int
   :return:

      - ``geom``: new flipped faces and edges

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: triangle_fill(bm, use_beauty, use_dissolve, edges, normal)

   Triangle Fill.

   Fill edges with triangles

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg use_beauty: Undocumented.
   :type use_beauty: bool
   :arg use_dissolve: dissolve resulting faces
   :type use_dissolve: bool
   :arg edges: input edges
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg normal: optionally pass the fill normal to use
   :type normal: :class:`mathutils.Vector` or any sequence of 3 floats
   :return:

      - ``geom``: new faces and edges

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: solidify(bm, geom, thickness)

   Solidify.

   Turns a mesh into a shell with thickness

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg geom: Undocumented.
   :type geom: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg thickness: Undocumented.
   :type thickness: float
   :return:

      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: inset_individual(bm, faces, thickness, depth, use_even_offset, use_interpolate, use_relative_offset)

   Face Inset (Individual).

   Insets individual faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg thickness: Undocumented.
   :type thickness: float
   :arg depth: Undocumented.
   :type depth: float
   :arg use_even_offset: Undocumented.
   :type use_even_offset: bool
   :arg use_interpolate: Undocumented.
   :type use_interpolate: bool
   :arg use_relative_offset: Undocumented.
   :type use_relative_offset: bool
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: inset_region(bm, faces, faces_exclude, use_boundary, use_even_offset, use_interpolate, use_relative_offset, use_edge_rail, thickness, depth, use_outset)

   Face Inset (Regions).

   Inset or outset face regions.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg faces_exclude: Undocumented.
   :type faces_exclude: list of (:class:`bmesh.types.BMFace`)
   :arg use_boundary: Undocumented.
   :type use_boundary: bool
   :arg use_even_offset: Undocumented.
   :type use_even_offset: bool
   :arg use_interpolate: Undocumented.
   :type use_interpolate: bool
   :arg use_relative_offset: Undocumented.
   :type use_relative_offset: bool
   :arg use_edge_rail: Undocumented.
   :type use_edge_rail: bool
   :arg thickness: Undocumented.
   :type thickness: float
   :arg depth: Undocumented.
   :type depth: float
   :arg use_outset: Undocumented.
   :type use_outset: bool
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: offset_edgeloops(bm, edges, use_cap_endpoint)

   Edgeloop Offset.

   Creates edge loops based on simple edge-outset method.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg edges: input faces
   :type edges: list of (:class:`bmesh.types.BMEdge`)
   :arg use_cap_endpoint: Undocumented.
   :type use_cap_endpoint: bool
   :return:

      - ``edges``: output faces

        **type** list of (:class:`bmesh.types.BMEdge`)

   :rtype: dict with string keys


.. function:: wireframe(bm, faces, thickness, offset, use_replace, use_boundary, use_even_offset, use_crease, crease_weight, thickness, use_relative_offset, material_offset)

   Wire Frame.

   Makes a wire-frame copy of faces.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg thickness: Undocumented.
   :type thickness: float
   :arg offset: Undocumented.
   :type offset: float
   :arg use_replace: Undocumented.
   :type use_replace: bool
   :arg use_boundary: Undocumented.
   :type use_boundary: bool
   :arg use_even_offset: Undocumented.
   :type use_even_offset: bool
   :arg use_crease: Undocumented.
   :type use_crease: bool
   :arg crease_weight: Undocumented.
   :type crease_weight: float
   :arg thickness: Undocumented.
   :type thickness: float
   :arg use_relative_offset: Undocumented.
   :type use_relative_offset: bool
   :arg material_offset: Undocumented.
   :type material_offset: int
   :return:

      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: poke(bm, faces, offset, center_mode, use_relative_offset)

   Pokes a face.

   Splits a face into a triangle fan.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg faces: input faces
   :type faces: list of (:class:`bmesh.types.BMFace`)
   :arg offset: center vertex offset along normal
   :type offset: float
   :arg center_mode: calculation mode for center vertex
   :type center_mode: int
   :arg use_relative_offset: apply offset
   :type use_relative_offset: bool
   :return:

      - ``verts``: output verts

        **type** list of (:class:`bmesh.types.BMVert`)
      - ``faces``: output faces

        **type** list of (:class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: convex_hull(bm, input, use_existing_faces)

   Convex Hull

   Builds a convex hull from the vertices in 'input'.

   If 'use_existing_faces' is true, the hull will not output triangles
   that are covered by a pre-existing face.

   All hull vertices, faces, and edges are added to 'geom.out'. Any
   input elements that end up inside the hull (i.e. are not used by an
   output face) are added to the 'interior_geom' slot. The
   'unused_geom' slot will contain all interior geometry that is
   completely unused. Lastly, 'holes_geom' contains edges and faces
   that were in the input and are part of the hull.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg input: Undocumented.
   :type input: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg use_existing_faces: Undocumented.
   :type use_existing_faces: bool
   :return:

      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom_interior``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom_unused``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
      - ``geom_holes``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


.. function:: symmetrize(bm, input, direction, dist)

   Symmetrize.

   Makes the mesh elements in the "input" slot symmetrical. Unlike
   normal mirroring, it only copies in one direction, as specified by
   the "direction" slot. The edges and faces that cross the plane of
   symmetry are split as needed to enforce symmetry.

   All new vertices, edges, and faces are added to the "geom.out" slot.

   :arg bm: The bmesh to operate on.
   :type bm: :class:`bmesh.types.BMesh`
   :arg input: Undocumented.
   :type input: list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)
   :arg direction: Undocumented.
   :type direction: int
   :arg dist: minimum distance
   :type dist: float
   :return:

      - ``geom``: 

        **type** list of (:class:`bmesh.types.BMVert`, :class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMFace`)

   :rtype: dict with string keys


