/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Based on Cem Yuksel. 2015. Sample Elimination for Generating Poisson Disk Sample
 * ! Sets. Computer Graphics Forum 34, 2 (May 2015), 25-32.
 * ! http://www.cemyuksel.com/research/sampleelimination/
 * Copyright (c) 2016, Cem Yuksel <cem@cemyuksel.com>
 * All rights reserved.
 */

#include "BLI_heap.h"
#include "BLI_kdtree.h"

#include "node_geometry_util.hh"

#include <iostream>

namespace blender::nodes {

static void tile_point(std::vector<float3> *tiled_points,
                       std::vector<size_t> *indices,
                       const float maximum_distance,
                       const float3 boundbox,
                       float3 const &point,
                       size_t index,
                       int dim = 0)
{
  for (int d = dim; d < 3; d++) {
    if (boundbox[d] - point[d] < maximum_distance) {
      float3 p = point;
      p[d] -= boundbox[d];

      tiled_points->push_back(p);
      indices->push_back(index);

      tile_point(tiled_points, indices, maximum_distance, boundbox, p, index, d + 1);
    }

    if (point[d] < maximum_distance) {
      float3 p = point;
      p[d] += boundbox[d];

      tiled_points->push_back(p);
      indices->push_back(index);

      tile_point(tiled_points, indices, maximum_distance, boundbox, p, index, d + 1);
    }
  }
}

/**
 * Returns the weight the point gets based on the distance to another point.
 */
static float point_weight_influence_get(const float maximum_distance,
                                        const float minimum_distance,
                                        const float squared_distance)
{
  const float alpha = 8.0f;
  float distance = std::sqrt(squared_distance);

  if (distance < minimum_distance) {
    distance = minimum_distance;
  }

  return std::pow(1.0f - distance / maximum_distance, alpha);
}

/**
 * Weight each point based on their proximity to its neighbors
 *
 * For each index in the weight array add a weight based on the proximity the
 * corresponding point has with its neighboors.
 **/
static void points_distance_weight_calculate(std::vector<float> *weights,
                                             const size_t point_id,
                                             Vector<float3> const *input_points,
                                             const void *kd_tree,
                                             const float minimum_distance,
                                             const float maximum_distance,
                                             Heap *heap,
                                             std::vector<HeapNode *> *nodes)
{
  KDTreeNearest_3d *nearest_point = nullptr;
  int neighbors = BLI_kdtree_3d_range_search(
      (KDTree_3d *)kd_tree, input_points->data()[point_id], &nearest_point, maximum_distance);

  for (int i = 0; i < neighbors; i++) {
    size_t neighbor_point_id = nearest_point[i].index;

    if (neighbor_point_id >= weights->size()) {
      continue;
    }

    /* The point should not influence itself. */
    if (neighbor_point_id == point_id) {
      continue;
    }

    const float squared_distance = nearest_point[i].dist * nearest_point[i].dist;
    const float weight_influence = point_weight_influence_get(
        maximum_distance, minimum_distance, squared_distance);

    /* In the first pass we just the weights. */
    if (heap == nullptr) {
      weights->data()[neighbor_point_id] += weight_influence;
    }
    /* When we run again we need to update the weights and the heap. */
    else {
      weights->data()[neighbor_point_id] -= weight_influence;
      HeapNode *node = nodes->data()[neighbor_point_id];
      if (node != nullptr) {
        BLI_heap_node_value_update(heap, node, weights->data()[neighbor_point_id]);
      }
    }
  }

  if (nearest_point) {
    MEM_freeN(nearest_point);
  }
}

/**
 * Returns the minimum radius fraction used by the default weight function.
 */
static float weight_limit_fraction_get(const size_t input_size, const size_t output_size)
{
  const float beta = 0.65f;
  const float gamma = 1.5f;
  float ratio = float(output_size) / float(input_size);
  return (1.0f - std::pow(ratio, gamma)) * beta;
}

/**
 * Tile the input points.
 */
static void points_tiling(Vector<float3> const *input_points,
                          const void *kd_tree,
                          const float maximum_distance,
                          const float3 boundbox)

{
  std::vector<float3> tiled_points(input_points->data(),
                                   input_points->data() + input_points->size());
  std::vector<size_t> indices(input_points->size());

  /* Start building a kdtree for the samples. */
  for (size_t i = 0; i < input_points->size(); i++) {
    indices[i] = i;
    BLI_kdtree_3d_insert((KDTree_3d *)kd_tree, i, input_points->data()[i]);
  }
  BLI_kdtree_3d_balance((KDTree_3d *)kd_tree);

  /* Tile the tree based on the boundbox. */
  for (size_t i = 0; i < input_points->size(); i++) {
    tile_point(&tiled_points, &indices, maximum_distance, boundbox, input_points->data()[i], i);
  }

  /* Re-use the same kdtree, so free it before re-creating it. */
  BLI_kdtree_3d_free((KDTree_3d *)kd_tree);

  /* Build a new tree with the new indices and tiled points. */
  kd_tree = BLI_kdtree_3d_new(tiled_points.size());
  for (size_t i = 0; i < tiled_points.size(); i++) {
    BLI_kdtree_3d_insert((KDTree_3d *)kd_tree, indices[i], tiled_points[i]);
  }
  BLI_kdtree_3d_balance((KDTree_3d *)kd_tree);
}

static void weighted_sample_elimination(Vector<float3> const *input_points,
                                        Vector<float3> *output_points,
                                        const float maximum_distance,
                                        const float3 boundbox)
{
  const float minimum_distance = maximum_distance *
                                 weight_limit_fraction_get(input_points->size(),
                                                           output_points->size());

  void *kd_tree = BLI_kdtree_3d_new(input_points->size());
  points_tiling(input_points, kd_tree, maximum_distance, boundbox);

  /* Assign weights to each sample. */
  std::vector<float> weights(input_points->size(), 0.0f);
  for (size_t point_id = 0; point_id < weights.size(); point_id++) {
    points_distance_weight_calculate(&weights,
                                     point_id,
                                     input_points,
                                     kd_tree,
                                     minimum_distance,
                                     maximum_distance,
                                     nullptr,
                                     nullptr);
  }

  /* Remove the points based on their weight. */
  Heap *heap = BLI_heap_new_ex(weights.size());
  std::vector<HeapNode *> nodes(input_points->size(), nullptr);

  for (size_t i = 0; i < weights.size(); i++) {
    nodes[i] = BLI_heap_insert(heap, weights[i], POINTER_FROM_INT(i));
  }

  size_t sample_size = input_points->size();
  while (sample_size > output_points->size()) {
    /* For each sample around it, remove its weight contribution and update the heap. */
    size_t point_id = POINTER_AS_INT(BLI_heap_pop_min(heap));
    nodes[point_id] = nullptr;

    points_distance_weight_calculate(&weights,
                                     point_id,
                                     input_points,
                                     kd_tree,
                                     minimum_distance,
                                     maximum_distance,
                                     heap,
                                     &nodes);

    sample_size--;
  }

  /* Copy the samples to the output array. */
  for (size_t i = 0; i < output_points->size(); i++) {
    size_t index = POINTER_AS_INT(BLI_heap_pop_min(heap));
    output_points->data()[i] = input_points->data()[index];
  }

  /* Cleanup. */
  BLI_kdtree_3d_free((KDTree_3d *)kd_tree);
  BLI_heap_free(heap, NULL);
}

void poisson_disk_point_elimination(Vector<float3> const *input_points,
                                    Vector<float3> *output_points,
                                    float maximum_density,
                                    float3 boundbox)
{
  weighted_sample_elimination(input_points, output_points, maximum_density, boundbox);

  /* Re-order the points for progressive sampling. */
#if 0
    std::vector<float3> tmpPoints(outputSize);
      float3 *inPts = outputPoints;
      float3 *outPts = tmpPoints.data();
      size_t inSize = outputSize;
      size_t outSize = 0;
      while (inSize >= 3) {
        outSize = inSize / 2;
        // dmax *= Sqrt(2.0f)
        d_max *= ProgressiveRadiusMultiplier(dimensions);
        //  weighted_sample_elimination(input_points, output_points, maximum_density, boundbox);
        DoEliminate(inPts, inSize, outPts, outSize, d_max, weightFunction, true);
        if (outPts != outputPoints) {
          MemCopy(outputPoints + outSize, outPts + outSize, inSize - outSize);
        }
        float3 *tmpPts = inPts;
        inPts = outPts;
        outPts = tmpPts;
        inSize = outSize;
      }
      if (inPts != outputPoints) {
        MemCopy(outputPoints, inPts, outSize);
      }
#endif
}

}  // namespace blender::nodes
