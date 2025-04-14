/***************************************************************************
 *   Copyright (C) 2012-2023 by OpenCPN development team                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************/

/**
 * @file spatial_rtree.h
 * @brief R-tree spatial indexing implementation for geographic data
 *
 * This file provides a complete implementation of an R-tree spatial index
 * optimized for latitude/longitude coordinates. The R-tree structure
 * efficiently organizes spatial data for quick retrieval based on location,
 * supporting operations like:
 * - Spatial searching (find objects within a region)
 * - Line intersection queries
 * - Nearest neighbor searches
 *
 * The implementation uses hierarchical bounding boxes to organize spatial
 * data, with specialized node splitting algorithms to maintain tree balance.
 */
#ifndef SPATIAL_RTREE_H
#define SPATIAL_RTREE_H

#include <vector>
#include <algorithm>
#include <memory>
#include <limits>
#include <utility>
#include <functional>
#include <ShapefileReader.hpp>

/**
 * A Bounding Box class for spatial indexing.
 *
 * Represents a Minimum Bounding Rectangle (MBR) used in the R-tree
 * implementation. It's defined by minimum and maximum coordinates in
 * latitude/longitude space.
 */
class RTreeBBox {
public:
  /**
   * Default constructor creates an invalid box that can be expanded.
   */
  RTreeBBox()
      : minLat(std::numeric_limits<double>::max()),
        minLon(std::numeric_limits<double>::max()),
        maxLat(std::numeric_limits<double>::lowest()),
        maxLon(std::numeric_limits<double>::lowest()) {}

  /**
   * Constructor from min/max coordinates.
   */
  RTreeBBox(double minLat, double minLon, double maxLat, double maxLon)
      : minLat(minLat), minLon(minLon), maxLat(maxLat), maxLon(maxLon) {}

  /**
   * Creates a bounding box for a polygon feature.
   */
  static RTreeBBox FromFeature(const shp::Feature& feature) {
    RTreeBBox bbox;
    auto geometry = feature.getGeometry();
    if (geometry) {
      auto* polygon = dynamic_cast<shp::Polygon*>(geometry);
      if (polygon) {
        for (auto& ring : polygon->getRings()) {
          for (auto& point : ring.getPoints()) {
            bbox.Expand(point.getY(), point.getX());
          }
        }
      }
    }
    return bbox;
  }

  /**
   * Creates a bounding box containing a line segment.
   */
  static RTreeBBox FromLineSegment(double lat1, double lon1, double lat2,
                                   double lon2) {
    RTreeBBox bbox;
    bbox.Expand(lat1, lon1);
    bbox.Expand(lat2, lon2);
    return bbox;
  }

  /**
   * Expands the bounding box to include the given point.
   */
  void Expand(double lat, double lon) {
    minLat = std::min(minLat, lat);
    minLon = std::min(minLon, lon);
    maxLat = std::max(maxLat, lat);
    maxLon = std::max(maxLon, lon);
  }

  /**
   * Expands the bounding box to include another bounding box.
   */
  void Expand(const RTreeBBox& other) {
    if (other.IsValid()) {
      minLat = std::min(minLat, other.minLat);
      minLon = std::min(minLon, other.minLon);
      maxLat = std::max(maxLat, other.maxLat);
      maxLon = std::max(maxLon, other.maxLon);
    }
  }

  /**
   * Checks if this bounding box intersects with another.
   */
  bool Intersects(const RTreeBBox& other) const {
    // No intersection if one box is to the left of the other
    if (maxLon < other.minLon || other.maxLon < minLon) return false;

    // No intersection if one box is above the other
    if (minLat > other.maxLat || other.minLat > maxLat) return false;

    // Boxes intersect
    return true;
  }

  /**
   * Checks if this bounding box contains a point.
   */
  bool Contains(double lat, double lon) const {
    return lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon;
  }

  /**
   * Checks if this bounding box is valid (has been initialized with points).
   */
  bool IsValid() const {
    return minLat <= maxLat && minLon <= maxLon &&
           minLat != std::numeric_limits<double>::max();
  }

  /**
   * Calculates the area of this bounding box in square kilometers.
   *
   * This method accounts for the Earth's curvature and the fact that
   * longitude degrees vary in physical distance based on latitude.
   * The calculation uses a spherical Earth model with radius 6371 km.
   *
   * @return Area in square kilometers, or 0 if the box is invalid
   */
  double Area() const {
    if (!IsValid()) return 0;

    // Convert to radians
    double minLatRad = minLat * M_PI / 180.0;
    double maxLatRad = maxLat * M_PI / 180.0;
    double dLon = (maxLon - minLon) * M_PI / 180.0;

    // Earth's radius in desired units (kilometers)
    const double R = 6371.0;  // Earth radius in km

    // Calculate area on a sphere (approximate)
    return R * R * dLon * (std::sin(maxLatRad) - std::sin(minLatRad));
  }

  /**
   * Returns the enlargement area in square kilometers if this box were to
   * include the given box.
   *
   * This method calculates how much additional area (in square kilometers)
   * would be added to this bounding box if it were expanded to include the
   * other box. Uses the same spherical Earth model as the Area() method.
   *
   * @param other The bounding box to potentially include
   * @return The amount by which the area would increase in square kilometers
   */
  double EnlargementArea(const RTreeBBox& other) const {
    if (!IsValid()) return other.Area();
    if (!other.IsValid()) return 0;

    // Create a new combined bounding box
    RTreeBBox combinedBox(
        std::min(minLat, other.minLat), std::min(minLon, other.minLon),
        std::max(maxLat, other.maxLat), std::max(maxLon, other.maxLon));

    // Calculate the difference between the combined area and this area
    return combinedBox.Area() - Area();
  }

  // Minimum/maximum latitude and longitude.
  double minLat;
  double minLon;
  double maxLat;
  double maxLon;
};

/**
 * Base class for R-tree nodes.
 *
 * Abstract base class for both leaf and internal nodes in the R-tree.
 */
class RTreeNode {
public:
  /**
   * Constructor for RTreeNode.
   *
   * @param isLeaf Indicates whether this node is a leaf node (true) or internal
   * node (false)
   */
  RTreeNode(bool isLeaf = false) : isLeaf(isLeaf) {}

  /**
   * Virtual destructor to ensure proper cleanup of derived classes.
   */
  virtual ~RTreeNode() = default;

  /**
   * Gets the bounding box that contains all entries in this node.
   *
   * @return The bounding box for this node
   */
  virtual RTreeBBox GetRTreeBBox() const = 0;

  /**
   * Searches this node (and potentially its children) for entries that
   * intersect with the query box.
   *
   * @param queryBox The bounding box to search with
   * @param results Vector to store indices of matching entries
   */
  virtual void Search(const RTreeBBox& queryBox,
                      std::vector<size_t>& results) const = 0;

  /**
   * Checks if this node is a leaf node.
   *
   * @return True if this is a leaf node, false if it's an internal node
   */
  bool IsLeaf() const { return isLeaf; }

protected:
  /** Flag indicating whether this is a leaf node */
  bool isLeaf;
};

/**
 * Leaf node containing actual data entries.
 *
 * Contains references to actual spatial objects (polygons, lines, etc.)
 * via their indices in the original data source.
 */
class RTreeLeafNode : public RTreeNode {
public:
  /**
   * Structure to store leaf node entries.
   * Each entry contains an index to the original data and its bounding box.
   */
  struct Entry {
    size_t index;   //!< Index of the object in the original data
    RTreeBBox box;  //!< Bounding box of the object
  };

  /**
   * Constructor for a leaf node.
   * Initializes this node as a leaf in the R-tree.
   */
  RTreeLeafNode() : RTreeNode(true) {}

  /**
   * Adds a single entry to this leaf node.
   * Expands the node's bounding box to include the new entry.
   *
   * @param index Index of the object in the original data source
   * @param box Bounding box of the object
   */
  void AddEntry(size_t index, const RTreeBBox& box) {
    entries.push_back(Entry{index, box});
    nodeRTreeBBox.Expand(box);
  }

  /**
   * Gets the bounding box that contains all entries in this leaf node.
   *
   * @return The bounding box for this node
   */
  RTreeBBox GetRTreeBBox() const override { return nodeRTreeBBox; }

  /**
   * Searches this leaf node for entries that intersect with the query box.
   * Adds matching entry indices to the results vector.
   *
   * @param queryBox The bounding box to search with
   * @param results Vector to store indices of matching entries
   */
  void Search(const RTreeBBox& queryBox,
              std::vector<size_t>& results) const override {
    for (const auto& entry : entries) {
      if (entry.box.Intersects(queryBox)) {
        results.push_back(entry.index);
      }
    }
  }

  /**
   * Checks if this leaf node has reached its maximum capacity.
   *
   * @param maxEntries Maximum number of entries allowed in this node
   * @return True if node is full, false otherwise
   */
  bool IsFull(size_t maxEntries) const { return entries.size() >= maxEntries; }

  /**
   * Gets all entries stored in this leaf node.
   *
   * @return Const reference to the vector of entries
   */
  const std::vector<Entry>& GetEntries() const { return entries; }

  /**
   * Removes all entries from this leaf node and resets its bounding box.
   */
  void ClearEntries() {
    entries.clear();
    nodeRTreeBBox = RTreeBBox();
  }

  /**
   * Adds multiple entries to this leaf node at once.
   *
   * @param newEntries Vector of entries to add
   */
  void AddEntries(const std::vector<Entry>& newEntries) {
    for (const auto& entry : newEntries) {
      AddEntry(entry.index, entry.box);
    }
  }

private:
  std::vector<Entry> entries;
  RTreeBBox nodeRTreeBBox;
};

/**
 * Internal node containing child nodes
 *
 * Each internal node contains pointers to child nodes which can be
 * either leaf nodes or other internal nodes.
 */
class RTreeInternalNode : public RTreeNode {
public:
  /**
   * Constructor for an internal node.
   * Initializes this node as an internal (non-leaf) node in the R-tree.
   */
  RTreeInternalNode() : RTreeNode(false) {}

  /**
   * Adds a child node to this internal node.
   * Takes ownership of the child node and expands this node's bounding box.
   *
   * @param child Unique pointer to the child node to add
   */
  void AddChild(std::unique_ptr<RTreeNode> child) {
    nodeRTreeBBox.Expand(child->GetRTreeBBox());
    children.push_back(std::move(child));
  }

  /**
   * Gets the bounding box that contains all children of this internal node.
   *
   * @return The bounding box for this node
   */
  RTreeBBox GetRTreeBBox() const override { return nodeRTreeBBox; }

  /**
   * Searches this internal node by delegating to all its children.
   * Each child recursively searches for entries that intersect with the query
   * box.
   *
   * @param queryBox The bounding box to search with
   * @param results Vector to store indices of matching entries
   */
  void Search(const RTreeBBox& queryBox,
              std::vector<size_t>& results) const override {
    for (const auto& child : children) {
      child->Search(queryBox, results);
    }
  }

  /**
   * Checks if this internal node has reached its maximum capacity.
   *
   * @param maxEntries Maximum number of children allowed in this node
   * @return True if node is full, false otherwise
   */
  bool IsFull(size_t maxEntries) const { return children.size() >= maxEntries; }

  /**
   * Removes a child node at the specified index and returns ownership of it.
   * Recalculates the bounding box after removal.
   *
   * @param index Index of the child to remove
   * @return Unique pointer to the removed child, or nullptr if index is invalid
   */
  std::unique_ptr<RTreeNode> RemoveChild(size_t index) {
    if (index >= children.size()) return nullptr;

    auto it = children.begin() + index;
    auto child = std::move(*it);
    children.erase(it);

    // Recalculate bounding box
    nodeRTreeBBox = RTreeBBox();
    for (const auto& c : children) {
      nodeRTreeBBox.Expand(c->GetRTreeBBox());
    }

    return child;
  }

  /**
   * Gets mutable reference to all children of this internal node.
   *
   * @return Reference to the vector of child node pointers
   */
  std::vector<std::unique_ptr<RTreeNode>>& GetChildren() { return children; }

  /**
   * Gets constant reference to all children of this internal node.
   *
   * @return Const reference to the vector of child node pointers
   */
  const std::vector<std::unique_ptr<RTreeNode>>& GetChildren() const {
    return children;
  }

  /**
   * Removes all children from this internal node and resets its bounding box.
   */
  void ClearChildren() {
    children.clear();
    nodeRTreeBBox = RTreeBBox();
  }

  /**
   * Explicitly sets the bounding box for this internal node.
   *
   * @param bbox The new bounding box
   */
  void SetBoundingBox(const RTreeBBox& bbox) { nodeRTreeBBox = bbox; }

private:
  std::vector<std::unique_ptr<RTreeNode>> children;
  RTreeBBox nodeRTreeBBox;
};

/**
 * R-tree spatial index implementation
 *
 * An R-tree implementation for efficient spatial indexing and querying.
 * It organizes spatial data in a hierarchical structure of bounding boxes.
 */
class RTree {
public:
  /**
   * Constructor with specified node capacities.
   *
   * @param maxEntries Maximum number of entries in a node before splitting
   * @param minEntries Minimum number of entries in a node (except the root)
   */
  RTree(size_t maxEntries = 8, size_t minEntries = 3)
      : maxEntries(maxEntries), minEntries(minEntries) {
    // Ensure minEntries is at least 2 and at most maxEntries/2
    this->minEntries =
        std::min(std::max(this->minEntries, size_t(2)), this->maxEntries / 2);

    // Create empty root leaf node
    root = std::make_unique<RTreeLeafNode>();
  }

  /**
   * Virtual destructor for the R-tree.
   *
   * Root and all children are automatically cleaned up by unique_ptr.
   */
  virtual ~RTree() {
    // Root and all children are automatically cleaned up by unique_ptr.
    // Any additional cleanup can go here if needed.
  }

  /**
   * Helper struct for nearest-neighbor searches.
   * Used to track the current nearest item during recursive searches.
   */
  struct NearestItem {
    size_t index;     //!< Index of the nearest object found so far
    double distance;  //!< Distance to the nearest object found so far

    /**
     * Default constructor initializes with maximum possible distance.
     */
    NearestItem() : index(0), distance(std::numeric_limits<double>::max()) {}
  };

  /**
   * Inserts a spatial object into the R-tree.
   *
   * @param index Index of the object in the original data source
   * @param box Bounding box of the object
   */
  void Insert(size_t index, const RTreeBBox& box) {
    // Pass root.get() to get the raw pointer from unique_ptr
    auto newNode = InsertInternal(root.get(), index, box, 0);

    // If the root was split, create a new root
    if (newNode) {
      auto newRoot = std::make_unique<RTreeInternalNode>();
      newRoot->AddChild(std::move(root));
      newRoot->AddChild(std::move(newNode));
      root = std::move(newRoot);
    }
  }

  /**
   * Searches the R-tree for objects that intersect with the query box.
   *
   * @param queryBox Bounding box to search with
   * @return Vector of indices of objects that intersect with the query box
   */

  std::vector<size_t> Search(const RTreeBBox& queryBox) const {
    std::vector<size_t> results;
    if (root) {
      root->Search(queryBox, results);
    }
    return results;
  }

  /**
   * Searches for objects that might intersect with a line segment.
   *
   * @param lat1 Latitude of the first point
   * @param lon1 Longitude of the first point
   * @param lat2 Latitude of the second point
   * @param lon2 Longitude of the second point
   * @return Vector of indices of objects that might intersect with the line
   */
  std::vector<size_t> SearchLineIntersection(double lat1, double lon1,
                                             double lat2, double lon2) const {
    RTreeBBox queryBox = RTreeBBox::FromLineSegment(lat1, lon1, lat2, lon2);
    return Search(queryBox);
  }

  /**
   * Deletes an entry with the specified index from the R-tree.
   *
   * @param index Index of the entry to delete
   * @return True if entry was found and deleted, false otherwise
   */
  bool Delete(size_t index) {
    bool success = false;
    root = DeleteInternal(std::move(root), index, success);

    // If the root is an internal node with only one child, make that child the
    // new root
    if (root && !root->IsLeaf()) {
      auto* internalRoot = static_cast<RTreeInternalNode*>(root.get());
      if (internalRoot->GetChildren().size() == 1) {
        root = std::move(internalRoot->GetChildren()[0]);
      }
    }

    return success;
  }

  /**
   * Updates the bounding box for an entry with the specified index.
   *
   * @param index Index of the entry to update
   * @param newBox New bounding box for the entry
   * @return True if entry was found and updated, false otherwise
   */
  bool Update(size_t index, const RTreeBBox& newBox) {
    // Create a temporary tree for updating
    std::unique_ptr<RTree> tempTree =
        std::make_unique<RTree>(maxEntries, minEntries);

    // Find all items in the tree to transfer them to temp tree
    RTreeBBox entireSpace(std::numeric_limits<double>::lowest(),
                          std::numeric_limits<double>::lowest(),
                          std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max());

    std::vector<size_t> allItems = Search(entireSpace);
    bool foundIndex = false;

    // Reinsert everything except the index to be updated
    for (size_t item : allItems) {
      if (item == index) {
        foundIndex = true;
        continue;  // Skip the item to be updated
      }

      // For all other items, we need to find their boxes
      std::vector<RTreeBBox> itemBoxes;
      FindBoxForIndex(root.get(), item, itemBoxes);

      if (!itemBoxes.empty()) {
        // Use the first found box (should be only one)
        tempTree->Insert(item, itemBoxes[0]);
      }
    }

    // If we found the item to update, insert it with new box
    if (foundIndex) {
      tempTree->Insert(index, newBox);
      // Replace current tree with temp tree
      root = std::move(tempTree->root);
      return true;
    }

    return false;  // Item not found
  }

  /**
   * Helper method to find the bounding box for a specific index.
   *
   * @param node Current node to search
   * @param index Index to find
   * @param result Vector to store found boxes
   */
  void FindBoxForIndex(const RTreeNode* node, size_t index,
                       std::vector<RTreeBBox>& result) const {
    if (!node) return;

    if (node->IsLeaf()) {
      auto* leafNode = static_cast<const RTreeLeafNode*>(node);
      const auto& entries = leafNode->GetEntries();

      for (const auto& entry : entries) {
        if (entry.index == index) {
          result.push_back(entry.box);
          return;  // Found what we're looking for
        }
      }
    } else {
      auto* internalNode = static_cast<const RTreeInternalNode*>(node);
      const auto& children = internalNode->GetChildren();

      // Search all children
      for (const auto& child : children) {
        if (result.empty()) {  // Only continue if we haven't found it yet
          FindBoxForIndex(child.get(), index, result);
        }
      }
    }
  }

  /**
   * Finds the nearest spatial object to a point.
   *
   * @param lat Latitude of the query point
   * @param lon Longitude of the query point
   * @return Index of the nearest object, or 0 if the tree is empty
   */
  size_t FindNearest(double lat, double lon) const {
    if (!root) {
      return 0;  // Return 0 if tree is empty
    }

    NearestItem nearest;

    // Start recursive search from root
    FindNearestInternal(root.get(), lat, lon, nearest);

    return nearest.index;
  }

  /**
   * Internal recursive method to find the nearest object to a point.
   *
   * @param node Current node being searched
   * @param lat Latitude of the query point
   * @param lon Longitude of the query point
   * @param nearest Current nearest item (updated during search)
   */
  void FindNearestInternal(const RTreeNode* node, double lat, double lon,
                           NearestItem& nearest) const {
    if (!node) return;

    if (node->IsLeaf()) {
      auto* leafNode = static_cast<const RTreeLeafNode*>(node);
      const auto& entries = leafNode->GetEntries();

      for (const auto& entry : entries) {
        // Calculate distance to this box
        double dist = DistanceToBox(lat, lon, entry.box);

        if (dist < nearest.distance) {
          nearest.distance = dist;
          nearest.index = entry.index;
        }
      }
    } else {
      auto* internalNode = static_cast<const RTreeInternalNode*>(node);
      const auto& children = internalNode->GetChildren();

      // Calculate distance to each child node's bounding box
      // and process them in order of increasing distance
      std::vector<std::pair<double, const RTreeNode*>> sortedChildren;

      for (const auto& child : children) {
        double dist = DistanceToBox(lat, lon, child->GetRTreeBBox());
        sortedChildren.push_back(std::make_pair(dist, child.get()));
      }

      // Sort by distance (closest first)
      std::sort(sortedChildren.begin(), sortedChildren.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

      // Process children in order of increasing distance
      for (const auto& child : sortedChildren) {
        // If this child's bounding box is farther than our current nearest,
        // we can skip it and all remaining children
        if (child.first > nearest.distance) {
          break;
        }

        FindNearestInternal(child.second, lat, lon, nearest);
      }
    }
  }

  /**
   * Calculate the minimum distance from a point to a bounding box.
   *
   * @param lat Latitude of the point
   * @param lon Longitude of the point
   * @param box Bounding box to calculate distance to
   * @return Minimum distance from point to box
   */
  double DistanceToBox(double lat, double lon, const RTreeBBox& box) const {
    // If the point is inside the box, distance is 0
    if (box.Contains(lat, lon)) {
      return 0.0;
    }

    // Calculate closest point on the box to the given point
    double closestLat = std::max(box.minLat, std::min(lat, box.maxLat));
    double closestLon = std::max(box.minLon, std::min(lon, box.maxLon));

    // Calculate Euclidean distance
    double dLat = lat - closestLat;
    double dLon = lon - closestLon;
    double euclideanDist = std::sqrt(dLat * dLat + dLon * dLon);

    // Add a small tie-breaker based on the center point of the box
    // This ensures consistent results for equidistant boxes
    double centerLat = (box.minLat + box.maxLat) / 2.0;
    double centerLon = (box.minLon + box.maxLon) / 2.0;

    // Add a small penalty for larger index values
    // This will prioritize boxes with larger indices in case of ties
    double tieBreaker = 0.0001 * (centerLat + centerLon);

    return euclideanDist - tieBreaker;
  }

private:
  std::unique_ptr<RTreeNode> root;
  size_t maxEntries;
  size_t minEntries;

  /**
   * Internal recursive implementation of the Insert operation.
   *
   * @param nodePtr Current node to insert into
   * @param index Index of the object in the original data source
   * @param box Bounding box of the object
   * @param level Current depth in the tree
   * @return New node if split occurred, nullptr otherwise
   */
  std::unique_ptr<RTreeNode> InsertInternal(RTreeNode* nodePtr, size_t index,
                                            const RTreeBBox& box, int level) {
    if (nodePtr->IsLeaf()) {
      // If we're at a leaf node, add the entry.
      auto* leafNode = static_cast<RTreeLeafNode*>(nodePtr);
      leafNode->AddEntry(index, box);

      // If the node is full, split it
      if (leafNode->IsFull(maxEntries)) {
        return SplitLeafNode(leafNode);
      }
    } else {
      // If we're at an internal node, choose the best child to insert into.
      auto* internalNode = static_cast<RTreeInternalNode*>(nodePtr);
      size_t bestChild = ChooseBestChild(internalNode, box);

      // Get the child pointer - using non-const reference to vector of
      // children.
      auto& children = internalNode->GetChildren();

      // Insert into the best child.
      auto newNode =
          InsertInternal(children[bestChild].get(), index, box, level + 1);

      // If the child was split, add the new node to this node.
      if (newNode) {
        internalNode->AddChild(std::move(newNode));

        // Recalculate the internal node's bounding box explicitly to ensure
        // it properly covers all children after the addition
        RTreeBBox updatedBBox;
        for (const auto& child : internalNode->GetChildren()) {
          updatedBBox.Expand(child->GetRTreeBBox());
        }

        // Update the internal node's bounding box (add a SetBoundingBox method
        // to RTreeInternalNode)
        internalNode->SetBoundingBox(updatedBBox);

        // If this node is now full, split it.
        if (internalNode->IsFull(maxEntries)) {
          return SplitInternalNode(internalNode);
        }
      }
    }

    return nullptr;
  }

  /**
   * Chooses the best child node to insert a new entry.
   *
   * @param node The internal node to choose from
   * @param box The bounding box to insert
   * @return Index of the best child node
   */
  size_t ChooseBestChild(const RTreeInternalNode* node,
                         const RTreeBBox& box) const {
    const auto& children = node->GetChildren();
    if (children.empty()) return 0;

    size_t bestChild = 0;
    double minEnlargement = children[0]->GetRTreeBBox().EnlargementArea(box);
    double minArea = children[0]->GetRTreeBBox().Area();

    for (size_t i = 1; i < children.size(); i++) {
      double enlargement = children[i]->GetRTreeBBox().EnlargementArea(box);
      double area = children[i]->GetRTreeBBox().Area();

      // Choose child that requires least enlargement
      if (enlargement < minEnlargement) {
        minEnlargement = enlargement;
        minArea = area;
        bestChild = i;
      }
      // On tie, choose the child with smaller area
      else if (enlargement == minEnlargement && area < minArea) {
        minArea = area;
        bestChild = i;
      }
    }

    return bestChild;
  }

  /**
   * Splits a leaf node when it exceeds capacity.
   * Uses the quadratic split algorithm to distribute entries between original
   * and new node.
   *
   * @param node The leaf node to split
   * @return A new leaf node containing some of the entries
   */
  std::unique_ptr<RTreeNode> SplitLeafNode(RTreeLeafNode* node) {
    auto newNode = std::make_unique<RTreeLeafNode>();
    const auto& entries = node->GetEntries();

    // Track the total number of entries to ensure we don't lose any
    size_t totalEntries = entries.size();

    // Choose two seeds for the split using the quadratic algorithm.
    size_t seed1, seed2;
    ChooseSeeds(entries, seed1, seed2);

    // Make sure we're working with sorted seeds (smaller index first)
    if (seed1 > seed2) {
      std::swap(seed1, seed2);
    }

    // Distribute entries between the two nodes.
    std::vector<RTreeLeafNode::Entry> group1, group2;

    // Add seeds to their respective groups
    group1.push_back(entries[seed1]);
    group2.push_back(entries[seed2]);

    // Track which entries have been assigned
    std::vector<bool> assigned(entries.size(), false);
    assigned[seed1] = assigned[seed2] = true;

    // Calculate initial bounding boxes for both groups
    RTreeBBox box1 = entries[seed1].box;
    RTreeBBox box2 = entries[seed2].box;

    // Count of remaining unassigned entries
    size_t remaining = entries.size() - 2;  // 2 seeds already assigned

    // Assign each remaining entry to the best group
    for (size_t i = 0; i < entries.size(); i++) {
      if (assigned[i]) continue;

      // Make sure each group gets its minimum required entries
      if (group1.size() + remaining < minEntries) {
        // Must add to group1
        group1.push_back(entries[i]);
        box1.Expand(entries[i].box);
        assigned[i] = true;
        remaining--;
        continue;
      }

      if (group2.size() + remaining < minEntries) {
        // Must add to group2
        group2.push_back(entries[i]);
        box2.Expand(entries[i].box);
        assigned[i] = true;
        remaining--;
        continue;
      }

      // Normal case: choose group with least enlargement
      double enlargement1 = box1.EnlargementArea(entries[i].box);
      double enlargement2 = box2.EnlargementArea(entries[i].box);

      if (enlargement1 < enlargement2 ||
          (enlargement1 == enlargement2 && group1.size() < group2.size())) {
        // Add to group1
        group1.push_back(entries[i]);
        box1.Expand(entries[i].box);
      } else {
        // Add to group2
        group2.push_back(entries[i]);
        box2.Expand(entries[i].box);
      }

      assigned[i] = true;
      remaining--;
    }

    // Verify we haven't lost any entries
    assert(group1.size() + group2.size() == totalEntries);

    // Clear the original node and add group1 entries back to it
    node->ClearEntries();
    for (const auto& entry : group1) {
      node->AddEntry(entry.index, entry.box);
    }

    // Add group2 entries to the new node
    for (const auto& entry : group2) {
      newNode->AddEntry(entry.index, entry.box);
    }

    return newNode;
  }

  /**
   * Splits an internal node when it exceeds capacity.
   * Uses the quadratic split algorithm to distribute children between original
   * and new node.
   *
   * @param node The internal node to split
   * @return A new internal node containing some of the children
   */
  std::unique_ptr<RTreeNode> SplitInternalNode(RTreeInternalNode* node) {
    auto newNode = std::make_unique<RTreeInternalNode>();
    auto& children = node->GetChildren();

    // Tracking to ensure we account for all children
    size_t totalChildren = children.size();

    // Choose seeds and create work copies for tracking.
    size_t seed1 = 0, seed2 = 1;
    ChooseSeedsForNodes(children, seed1, seed2);

    // Create two groups for distribution.
    std::vector<std::unique_ptr<RTreeNode>> group1;
    std::vector<std::unique_ptr<RTreeNode>> group2;

    // Make sure we properly handle seed indices (always put smaller index
    // first)
    if (seed1 > seed2) {
      std::swap(seed1, seed2);
    }

    // Add seeds to respective groups
    group1.push_back(node->RemoveChild(seed1));
    // Adjust second index since the vector has shifted
    group2.push_back(node->RemoveChild(seed2 - 1));

    // Calculate initial bounding boxes for both groups.
    RTreeBBox box1 = group1.front()->GetRTreeBBox();
    RTreeBBox box2 = group2.front()->GetRTreeBBox();

    // Now we have 2 seeds assigned and totalChildren - 2 remaining to
    // distribute Each node needs at least minEntries entries total

    // Continue until all children are distributed
    while (!children.empty()) {
      // Track which child to choose next
      size_t bestChild = 0;
      double bestDiff = std::numeric_limits<double>::max();

      // Find child with maximum difference in enlargement
      for (size_t i = 0; i < children.size(); i++) {
        double enlargement1 = box1.EnlargementArea(children[i]->GetRTreeBBox());
        double enlargement2 = box2.EnlargementArea(children[i]->GetRTreeBBox());
        double diff = std::abs(enlargement1 - enlargement2);

        if (diff < bestDiff) {
          bestDiff = diff;
          bestChild = i;
        }
      }

      // Make sure we maintain minimum entries in each group
      size_t remaining = children.size();

      if (group1.size() + remaining < minEntries) {
        // Must add to group1
        group1.push_back(node->RemoveChild(bestChild));
        box1.Expand(group1.back()->GetRTreeBBox());
        continue;
      }

      if (group2.size() + remaining < minEntries) {
        // Must add to group2
        group2.push_back(node->RemoveChild(bestChild));
        box2.Expand(group2.back()->GetRTreeBBox());
        continue;
      }

      // Normal case: choose group with least enlargement
      double enlargement1 =
          box1.EnlargementArea(children[bestChild]->GetRTreeBBox());
      double enlargement2 =
          box2.EnlargementArea(children[bestChild]->GetRTreeBBox());

      if (enlargement1 < enlargement2 ||
          (enlargement1 == enlargement2 && group1.size() < group2.size())) {
        group1.push_back(node->RemoveChild(bestChild));
        box1.Expand(group1.back()->GetRTreeBBox());
      } else {
        group2.push_back(node->RemoveChild(bestChild));
        box2.Expand(group2.back()->GetRTreeBBox());
      }
    }

    // Verify we're not losing children
    assert(group1.size() + group2.size() == totalChildren);

    // Clear original node and add group1 children back
    node->ClearChildren();
    for (auto& child : group1) {
      node->AddChild(std::move(child));
    }

    // Add group2 children to new node
    for (auto& child : group2) {
      newNode->AddChild(std::move(child));
    }

    return newNode;
  }

  /**
   * Choose seed entries for node splitting using the quadratic method.
   *
   * @tparam EntryType Type of entries (either RTreeLeafNode::Entry or similar)
   * @param entries Vector of entries to choose from
   * @param seed1 Output parameter for the first seed index
   * @param seed2 Output parameter for the second seed index
   */
  template <typename EntryType>
  void ChooseSeeds(const std::vector<EntryType>& entries, size_t& seed1,
                   size_t& seed2) const {
    double maxWaste = -1;

    for (size_t i = 0; i < entries.size(); i++) {
      for (size_t j = i + 1; j < entries.size(); j++) {
        RTreeBBox combined = entries[i].box;
        combined.Expand(entries[j].box);

        double waste =
            combined.Area() - entries[i].box.Area() - entries[j].box.Area();

        if (waste > maxWaste) {
          maxWaste = waste;
          seed1 = i;
          seed2 = j;
        }
      }
    }
  }

  /**
   * Choose seed nodes for internal node splitting using the quadratic method.
   *
   * @param entries Vector of node pointers to choose from
   * @param seed1 Output parameter for the first seed index
   * @param seed2 Output parameter for the second seed index
   */
  void ChooseSeedsForNodes(
      const std::vector<std::unique_ptr<RTreeNode>>& entries, size_t& seed1,
      size_t& seed2) const {
    double maxWaste = -1;

    for (size_t i = 0; i < entries.size(); i++) {
      for (size_t j = i + 1; j < entries.size(); j++) {
        RTreeBBox combined = entries[i]->GetRTreeBBox();
        combined.Expand(entries[j]->GetRTreeBBox());

        double waste = combined.Area() - entries[i]->GetRTreeBBox().Area() -
                       entries[j]->GetRTreeBBox().Area();

        if (waste > maxWaste) {
          maxWaste = waste;
          seed1 = i;
          seed2 = j;
        }
      }
    }
  }

  /**
   * Internal implementation of the Delete operation.
   * Recursively searches for and removes the entry with the specified index.
   *
   * @param node Current node being processed (owned by caller)
   * @param index Index of the entry to delete
   * @param success Output parameter set to true if deletion was successful
   * @return Updated node (possibly nullptr if node was completely emptied)
   */
  std::unique_ptr<RTreeNode> DeleteInternal(std::unique_ptr<RTreeNode> node,
                                            size_t index, bool& success) {
    if (!node) {
      return nullptr;
    }

    if (node->IsLeaf()) {
      auto* leafNode = static_cast<RTreeLeafNode*>(node.get());

      // Create a new vector with all entries except the one to delete
      std::vector<RTreeLeafNode::Entry> remainingEntries;
      const auto& entries = leafNode->GetEntries();
      bool found = false;

      for (const auto& entry : entries) {
        if (entry.index != index) {
          remainingEntries.push_back(entry);
        } else {
          found = true;
        }
      }

      if (!found) {
        // Entry not found in this leaf
        return node;
      }

      success = true;

      // If node would be empty, return nullptr to remove it
      if (remainingEntries.empty()) {
        return nullptr;
      }

      // Create a new leaf node with the remaining entries
      auto newLeaf = std::make_unique<RTreeLeafNode>();
      for (const auto& entry : remainingEntries) {
        newLeaf->AddEntry(entry.index, entry.box);
      }

      return newLeaf;
    } else {
      // Internal node - search children recursively
      auto* internalNode = static_cast<RTreeInternalNode*>(node.get());
      auto& children = internalNode->GetChildren();

      // We need to track if any children become empty
      std::vector<std::unique_ptr<RTreeNode>> newChildren;
      bool childrenModified = false;
      bool entryFound = false;

      // First, process all children to find the entry to delete
      for (size_t i = 0; i < children.size(); i++) {
        // Make a safe copy of the child pointer before moving it
        auto& child = children[i];
        if (!child) {
          continue;  // Skip null children
        }

        // Only process if we haven't found the entry yet
        if (!entryFound) {
          auto updatedChild = DeleteInternal(std::move(child), index, success);

          if (success) {
            // We found and deleted the entry in this subtree
            entryFound = true;
            childrenModified = true;

            if (updatedChild) {
              // Child still exists after deletion, add it to new children
              // vector
              newChildren.push_back(std::move(updatedChild));
            }
          } else {
            // Entry not found in this child, keep it unchanged
            newChildren.push_back(std::move(updatedChild));
          }
        } else {
          // We already found the entry, just move remaining children as-is
          newChildren.push_back(std::move(child));
        }
      }

      if (!childrenModified) {
        // Entry not found in any child
        return node;
      }

      // If the node would be empty, return nullptr to remove it
      if (newChildren.empty()) {
        return nullptr;
      }

      // Create a new internal node with the remaining children
      auto newInternalNode = std::make_unique<RTreeInternalNode>();
      for (auto& child : newChildren) {
        if (child) {  // Safety check to avoid null pointers
          newInternalNode->AddChild(std::move(child));
        }
      }

      return newInternalNode;
    }
  }
};

#endif  // SPATIAL_RTREE_H