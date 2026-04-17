//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_B_STAR_PLUS_TREE_H
#define QUASARDB_B_STAR_PLUS_TREE_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <vector>
#include "internal_node.h"
#include "leaf_node.h"

template <typename KeyType, typename ValueType>
class BStarPlusTree final {
private:
    Node<KeyType, ValueType>* root;
    LeafNode<KeyType, ValueType>* firstLeaf;

public:
    BStarPlusTree() : root(nullptr), firstLeaf(nullptr) {}
    BStarPlusTree(const BStarPlusTree&) = delete;
    BStarPlusTree& operator=(const BStarPlusTree&) = delete;

    ~BStarPlusTree() {
        clear(root);
        firstLeaf = nullptr;
    }

    std::optional<ValueType> search(const KeyType& key) const {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::search(key=" << key << ")" << std::endl;
        }
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        if (!leaf) {
            return std::nullopt;
        }
        return leaf->search(key);
    }

    void insert(const KeyType& key, const ValueType& value) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::insert(key=" << key << ", value=" << value << ")" << std::endl;
        }
        if (!root) {
            root = new LeafNode<KeyType, ValueType>();
            firstLeaf = static_cast<LeafNode<KeyType, ValueType>*>(root);
            firstLeaf->insert(key, value);
            return;
        }

        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        assert(leaf != nullptr);

        bool overflow = leaf->insert(key, value);
        if (!overflow) {
            return;
        }

        if (leaf->size() > Node<KeyType, ValueType>::MAX_KEYS) {
            handleLeafOverflow(leaf, parentStack);
        }
    }

    bool remove(const KeyType& key) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::remove(key=" << key << ")" << std::endl;
        }
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        if (!leaf) {
            return false;
        }
        bool removed = leaf->remove(key);
        if (!removed) {
            return false;
        }
        if (leaf == root) {
            if (leaf->keys.empty()) {
                delete root;
                root = nullptr;
                firstLeaf = nullptr;
            }
            return true;
        }
        if (leaf->underflow()) {
            handleLeafUnderflow(leaf, parentStack);
        }
        return true;
    }

    std::vector<std::pair<KeyType, ValueType>> rangeSearch(const KeyType& low, const KeyType& high) const {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::rangeSearch(low=" << low << ", high=" << high << ")" << std::endl;
        }
        std::vector<std::pair<KeyType, ValueType>> result;
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>> parentStack;
        auto leaf = findLeaf(low, parentStack);
        for (; leaf; leaf = leaf->next) {
            for (size_t i = 0; i < leaf->size(); ++i) {
                if (leaf->keys[i] > high) {
                    return result;
                }
                if (leaf->keys[i] >= low) {
                    result.emplace_back(leaf->keys[i], leaf->values[i]);
                }
            }
        }
        return result;
    }

    void print(std::ostream& os = std::cout) const { printNode(os, root, 0); }

private:
    void clear(Node<KeyType, ValueType>* node) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::clear(node=" << node << ")" << std::endl;
        }
        if (!node) {
            return;
        }
        if (!node->isLeaf()) {
            auto in = static_cast<InternalNode<KeyType, ValueType>*>(node);
            for (auto* child : in->children) {
                clear(child);
            }
        }
        delete node;
    }

    LeafNode<KeyType, ValueType>* findLeaf(
        const KeyType& key,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) const {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::findLeaf(key=" << key << ")" << std::endl;
        }
        Node<KeyType, ValueType>* cur = root;
        while (cur && !cur->isLeaf()) {
            auto in = static_cast<InternalNode<KeyType, ValueType>*>(cur);
            int idx = std::upper_bound(in->keys.begin(), in->keys.end(), key) - in->keys.begin();
            parentStack.push({in, idx});
            cur = in->children[idx];
        }
        return static_cast<LeafNode<KeyType, ValueType>*>(cur);
    }

    void handleLeafOverflow(
        LeafNode<KeyType, ValueType>* leaf,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::handleLeafOverflow(leaf=" << leaf << ")" << std::endl;
        }

        if (leaf == root) {
            splitLeaf1to2(leaf);
            return;
        }
        auto [parent, indexInParent] = parentStack.top();
        LeafNode<KeyType, ValueType>* leftSibling = nullptr;
        if (indexInParent > 0) {
            leftSibling = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[indexInParent - 1]);
            assert(leftSibling->isLeaf());
        }
        if (leftSibling && leftSibling->size() < Node<KeyType, ValueType>::MAX_KEYS) {
            redistributeLeaves2to2(leftSibling, leaf, parent, indexInParent - 1);
            return;
        }
        LeafNode<KeyType, ValueType>* rightSibling = nullptr;
        if (indexInParent + 1 < parent->children.size()) {
            rightSibling = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[indexInParent + 1]);
            assert(rightSibling->isLeaf());
        }
        if (rightSibling && rightSibling->size() < Node<KeyType, ValueType>::MAX_KEYS) {
            redistributeLeaves2to2(leaf, rightSibling, parent, indexInParent);
            return;
        }
        splitLeaves2to3(leaf, parentStack);
    }

    void splitLeaf1to2(LeafNode<KeyType, ValueType>* node) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::splitLeaf1to2(node=" << node << ")" << std::endl;
        }

        auto newRoot = new InternalNode<KeyType, ValueType>();
        newRoot->children.push_back(node);
        root = newRoot;

        auto newNode = new LeafNode<KeyType, ValueType>();
        int mid = node->size() / 2;
        KeyType pushKey = node->keys[mid];

        newNode->keys.assign(node->keys.begin() + mid, node->keys.end());
        newNode->values.assign(node->values.begin() + mid, node->values.end());
        node->keys.erase(node->keys.begin() + mid, node->keys.end());
        node->values.erase(node->values.begin() + mid, node->values.end());

        node->next = newNode;
        newNode->prev = node;

        bool overflow = static_cast<InternalNode<KeyType, ValueType>*>(root)->insertKey(pushKey, newNode);
        assert(!overflow);
    }

    void redistributeLeaves2to2(
        LeafNode<KeyType, ValueType>* left,
        LeafNode<KeyType, ValueType>* right,
        InternalNode<KeyType, ValueType>* parent,
        int leftIndex
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout
                << "BTree::redistributeLeaves2to2(left=" << left << ", right=" << right << ", parent=" << parent
                << ", leftInx=" << leftIndex << ")" << std::endl;
        }

        std::vector<KeyType> allK = std::move(left->keys);
        std::vector<ValueType> allV = std::move(left->values);
        allK.insert(allK.end(), right->keys.begin(), right->keys.end());
        allV.insert(allV.end(), right->values.begin(), right->values.end());

        size_t leftSize = allK.size() / 2;

        left->keys.assign(allK.begin(), allK.begin() + leftSize);
        left->values.assign(allV.begin(), allV.begin() + leftSize);
        right->keys.assign(allK.begin() + leftSize, allK.end());
        right->values.assign(allV.begin() + leftSize, allV.end());

        parent->keys[leftIndex] = right->keys[0];
    }

    void redistributeLeaves3to3(
        LeafNode<KeyType, ValueType>* left,
        LeafNode<KeyType, ValueType>* middle,
        LeafNode<KeyType, ValueType>* right,
        InternalNode<KeyType, ValueType>* parent,
        int leftIndex
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout
                << "BTree::redistributeLeaves3to3(left=" << left << ", middle=" << middle << ", right=" << right
                << ", parent=" << parent << ", leftInx=" << leftIndex << ")" << std::endl;
        }

        std::vector<KeyType> allK = std::move(left->keys);
        std::vector<ValueType> allV = std::move(left->values);
        allK.insert(allK.end(), middle->keys.begin(), middle->keys.end());
        allV.insert(allV.end(), middle->values.begin(), middle->values.end());
        allK.insert(allK.end(), right->keys.begin(), right->keys.end());
        allV.insert(allV.end(), right->values.begin(), right->values.end());

        size_t total = allK.size();
        size_t s1 = total / 3;
        size_t s2 = (total - s1) / 2;

        left->keys.assign(allK.begin(), allK.begin() + s1);
        left->values.assign(allV.begin(), allV.begin() + s1);
        middle->keys.assign(allK.begin() + s1, allK.begin() + s1 + s2);
        middle->values.assign(allV.begin() + s1, allV.begin() + s1 + s2);
        right->keys.assign(allK.begin() + s1 + s2, allK.end());
        right->values.assign(allV.begin() + s1 + s2, allV.end());

        parent->keys[leftIndex] = middle->keys[0];
        parent->keys[leftIndex + 1] = right->keys[0];
    }

    void splitLeaves2to3(
        LeafNode<KeyType, ValueType>* middle,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::splitLeaves2to3(middle=" << middle << ")" << std::endl;
        }

        auto [parent, idxInParent] = parentStack.top();
        LeafNode<KeyType, ValueType>* left = nullptr;
        LeafNode<KeyType, ValueType>* right = nullptr;
        if (idxInParent > 0) {
            left = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[idxInParent - 1]);
        } else if (idxInParent + 1 < (int)parent->children.size()) {
            right = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[idxInParent + 1]);
        }
        assert(left || right);

        auto n1 = (left) ? left : middle;
        auto n2 = (left) ? middle : right;

        std::vector<KeyType> allK = std::move(n1->keys);
        std::vector<ValueType> allV = std::move(n1->values);

        allK.insert(allK.end(), n2->keys.begin(), n2->keys.end());
        allV.insert(allV.end(), n2->values.begin(), n2->values.end());

        auto n3 = new LeafNode<KeyType, ValueType>();

        size_t total = allK.size();
        size_t s1 = total / 3;
        size_t s2 = (total - s1) / 2;
        size_t s3 = total - s1 - s2;

        auto assign = [&](LeafNode<KeyType, ValueType>* n, size_t start, size_t len) {
            n->keys.assign(allK.begin() + start, allK.begin() + start + len);
            n->values.assign(allV.begin() + start, allV.begin() + start + len);
        };
        assign(n1, 0, s1);
        assign(n2, s1, s2);
        assign(n3, s1 + s2, s3);

        n3->next = n2->next;
        if (n3->next) {
            n3->next->prev = n3;
        }
        n2->next = n3;
        n3->prev = n2;

        int n2Idx = (left) ? idxInParent : idxInParent + 1;
        parent->keys[n2Idx - 1] = n2->keys[0];
        parent->insertKey(n3->keys[0], n3);

        if (parent->size() > Node<KeyType, ValueType>::MAX_KEYS) {
            parentStack.pop();
            handleInternalOverflow(parent, parentStack);
        }
    }

    void handleInternalOverflow(
        InternalNode<KeyType, ValueType>* node,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::handleInternalOverflow(node=" << node << ")" << std::endl;
        }

        if (node == root) {
            splitInternal1to2(node);
            return;
        }

        auto [parent, idx] = parentStack.top();

        if (idx > 0) {
            auto left = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx - 1]);
            if (left->size() < Node<KeyType, ValueType>::MAX_KEYS) {
                redistributeInternal2to2(left, node, parent, idx - 1);
                return;
            }
        }
        if (idx < (int)parent->children.size() - 1) {
            auto right = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx + 1]);
            if (right->size() < Node<KeyType, ValueType>::MAX_KEYS) {
                redistributeInternal2to2(node, right, parent, idx);
                return;
            }
        }
        splitInternal2to3(node, parentStack);
    }

    void redistributeInternal2to2(
        InternalNode<KeyType, ValueType>* left,
        InternalNode<KeyType, ValueType>* right,
        InternalNode<KeyType, ValueType>* parent,
        int leftIdxInParent
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout
                << "BTree::redistributeInternal2to2(left=" << left << ", right=" << right << ", parent=" << parent
                << ", leftIdxInParent=" << leftIdxInParent << ")" << std::endl;
        }

        std::vector<KeyType> allK = std::move(left->keys);
        allK.push_back(parent->keys[leftIdxInParent]);
        allK.insert(allK.end(), right->keys.begin(), right->keys.end());

        std::vector<Node<KeyType, ValueType>*> allC = std::move(left->children);
        allC.insert(allC.end(), right->children.begin(), right->children.end());

        int mid = allK.size() / 2;

        left->keys.assign(allK.begin(), allK.begin() + mid);
        left->children.assign(allC.begin(), allC.begin() + mid + 1);
        parent->keys[leftIdxInParent] = allK[mid];
        right->keys.assign(allK.begin() + mid + 1, allK.end());
        right->children.assign(allC.begin() + mid + 1, allC.end());
    }

    void redistributeInternal3to3(
        InternalNode<KeyType, ValueType>* left,
        InternalNode<KeyType, ValueType>* middle,
        InternalNode<KeyType, ValueType>* right,
        InternalNode<KeyType, ValueType>* parent,
        int leftIdxInParent
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout
                << "BTree::redistributeInternal3to3(left=" << left << ", middle=" << middle << ", right=" << right
                << ", parent=" << parent << ", leftIdxInParent=" << leftIdxInParent << ")" << std::endl;
        }

        std::vector<KeyType> allK = std::move(left->keys);
        allK.push_back(parent->keys[leftIdxInParent]);
        allK.insert(allK.end(), middle->keys.begin(), middle->keys.end());
        allK.push_back(parent->keys[leftIdxInParent + 1]);
        allK.insert(allK.end(), right->keys.begin(), right->keys.end());

        std::vector<Node<KeyType, ValueType>*> allC = std::move(left->children);
        allC.insert(allC.end(), middle->children.begin(), middle->children.end());
        allC.insert(allC.end(), right->children.begin(), right->children.end());

        int total = allK.size();
        int s1 = total / 3;
        int s2 = (total - s1 - 1) / 2;

        left->keys.assign(allK.begin(), allK.begin() + s1);
        left->children.assign(allC.begin(), allC.begin() + s1 + 1);
        parent->keys[leftIdxInParent] = allK[s1];
        middle->keys.assign(allK.begin() + s1 + 1, allK.begin() + s1 + 1 + s2);
        middle->children.assign(allC.begin() + s1 + 1, allC.begin() + s1 + 1 + s2 + 1);
        parent->keys[leftIdxInParent + 1] = allK[s1 + 1 + s2];
        right->keys.assign(allK.begin() + s1 + 1 + s2 + 1, allK.end());
        right->children.assign(allC.begin() + s1 + 1 + s2 + 1, allC.end());
    }

    void splitInternal2to3(
        InternalNode<KeyType, ValueType>* node,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::splitInternal2to3(node=" << node << ")" << std::endl;
        }

        auto [parent, idx] = parentStack.top();
        int leftIdx = (idx > 0) ? idx - 1 : idx;
        auto n1 = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[leftIdx]);
        auto n2 = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[leftIdx + 1]);
        auto n3 = new InternalNode<KeyType, ValueType>();

        std::vector<KeyType> allK = std::move(n1->keys);
        allK.push_back(parent->keys[leftIdx]);
        allK.insert(allK.end(), n2->keys.begin(), n2->keys.end());

        std::vector<Node<KeyType, ValueType>*> allC = std::move(n1->children);
        allC.insert(allC.end(), n2->children.begin(), n2->children.end());

        int total = allK.size();
        int s1 = total / 3;
        int s2 = (total - s1 - 1) / 2;

        n1->keys.assign(allK.begin(), allK.begin() + s1);
        n1->children.assign(allC.begin(), allC.begin() + s1 + 1);
        KeyType sep1 = allK[s1];

        n2->keys.assign(allK.begin() + s1 + 1, allK.begin() + s1 + 1 + s2);
        n2->children.assign(allC.begin() + s1 + 1, allC.begin() + s1 + 1 + s2 + 1);
        KeyType sep2 = allK[s1 + 1 + s2];

        n3->keys.assign(allK.begin() + s1 + 1 + s2 + 1, allK.end());
        n3->children.assign(allC.begin() + s1 + 1 + s2 + 1, allC.end());

        parent->keys[leftIdx] = sep1;
        parent->insertKey(sep2, n3);

        if (parent->size() > Node<KeyType, ValueType>::MAX_KEYS) {
            parentStack.pop();
            handleInternalOverflow(parent, parentStack);
        }
    }

    void splitInternal1to2(InternalNode<KeyType, ValueType>* node) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::splitInternal1to2(node=" << node << ")" << std::endl;
        }

        auto newRoot = new InternalNode<KeyType, ValueType>();
        newRoot->children.push_back(node);
        root = newRoot;

        auto newNode = new InternalNode<KeyType, ValueType>();
        int mid = node->size() / 2;
        KeyType pushKey = node->keys[mid];

        newNode->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
        newNode->children.assign(node->children.begin() + mid + 1, node->children.end());
        node->keys.erase(node->keys.begin() + mid, node->keys.end());
        node->children.erase(node->children.begin() + mid + 1, node->children.end());

        bool overflow = static_cast<InternalNode<KeyType, ValueType>*>(root)->insertKey(pushKey, newNode);
        assert(!overflow);
    }

    void handleLeafUnderflow(
        LeafNode<KeyType, ValueType>* leaf,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::handleLeafUnderflow(leaf=" << leaf << ")" << std::endl;
        }

        if (parentStack.empty()) {
            return;
        }

        auto [parent, index] = parentStack.top();

        if (index > 0) {
            auto left = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index - 1]);
            if (left->canLend()) {
                redistributeLeaves2to2(left, leaf, parent, index - 1);
                return;
            }
        }
        if (index + 1 < parent->children.size()) {
            auto right = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index + 1]);
            if (right->canLend()) {
                redistributeLeaves2to2(leaf, right, parent, index);
                return;
            }
        }
        if (parent->children.size() == 2) {
            assert(parent == root);
            if (parent->children[0]->size() + parent->children[1]->size() < Node<KeyType, ValueType>::MAX_KEYS) {
                mergeLeaves2to1(leaf, parentStack);
            }
            return;
        }
        if (index == 0 && parent->children[index + 2]->canLend()) {
            redistributeLeaves3to3(
                leaf,
                static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index + 1]),
                static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index + 2]),
                parent,
                index
            );
            return;
        }
        if (index == parent->children.size() - 1 && parent->children[parent->children.size() - 3]->canLend()) {
            redistributeLeaves3to3(
                static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index - 2]),
                static_cast<LeafNode<KeyType, ValueType>*>(parent->children[index - 1]),
                leaf,
                parent,
                index - 2
            );
            return;
        }
        mergeLeaves3to2(leaf, parentStack);
    }

    void mergeLeaves3to2(
        LeafNode<KeyType, ValueType>* leaf,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::mergeLeaves3to2(leaf=" << leaf << ")" << std::endl;
        }

        auto [parent, idxInParent] = parentStack.top();
        int shift;
        if (idxInParent == 0) {
            shift = 0;
        } else if (idxInParent == (int)parent->children.size() - 1) {
            shift = -2;
        } else {
            shift = -1;
        }
        auto n1 = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[idxInParent + shift]);
        auto n2 = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[idxInParent + shift + 1]);
        auto n3 = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[idxInParent + shift + 2]);

        std::vector<KeyType> allK = std::move(n1->keys);
        std::vector<ValueType> allV = std::move(n1->values);

        auto collect = [&](LeafNode<KeyType, ValueType>* n) {
            allK.insert(allK.end(), n->keys.begin(), n->keys.end());
            allV.insert(allV.end(), n->values.begin(), n->values.end());
        };
        collect(n2);
        collect(n3);

        size_t s1 = allK.size() / 2;

        n1->keys.assign(allK.begin(), allK.begin() + s1);
        n1->values.assign(allV.begin(), allV.begin() + s1);
        n2->keys.assign(allK.begin() + s1, allK.end());
        n2->values.assign(allV.begin() + s1, allV.end());

        n2->next = n3->next;
        if (n2->next) {
            n2->next->prev = n2;
        }

        parent->keys[idxInParent + shift] = n2->keys[0];
        parent->keys.erase(parent->keys.begin() + idxInParent + shift + 1);
        parent->children.erase(parent->children.begin() + idxInParent + shift + 2);
        delete n3;

        if (parent != root && parent->underflow()) {
            print();
            parentStack.pop();
            handleInternalUnderflow(parent, parentStack);
        }
    }

    void mergeLeaves2to1(
        LeafNode<KeyType, ValueType>* leaf,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::mergeLeaves2to1(leaf=" << leaf << ")" << std::endl;
        }

        auto [parent, idxInParent] = parentStack.top();

        int leftIdx = (idxInParent == 0) ? 0 : idxInParent - 1;
        auto n1 = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[leftIdx]);
        auto n2 = static_cast<LeafNode<KeyType, ValueType>*>(parent->children[leftIdx + 1]);

        n1->keys.insert(n1->keys.end(), n2->keys.begin(), n2->keys.end());
        n1->values.insert(n1->values.end(), n2->values.begin(), n2->values.end());

        n1->next = n2->next;
        assert(n1->next == nullptr);

        parent->keys.erase(parent->keys.begin() + leftIdx);
        parent->children.erase(parent->children.begin() + leftIdx + 1);

        delete n2;
        root = n1;
        delete parent;
    }

    void handleInternalUnderflow(
        InternalNode<KeyType, ValueType>* node,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::handleInternalUnderflow(node=" << node << ")" << std::endl;
        }

        assert(node != root);
        auto [parent, index] = parentStack.top();

        if (index > 0) {
            auto left = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index - 1]);
            if (left->canLend()) {
                redistributeInternal2to2(left, node, parent, index - 1);
                return;
            }
        }
        if (index + 1 < parent->children.size()) {
            auto right = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index + 1]);
            if (right->canLend()) {
                redistributeInternal2to2(node, right, parent, index);
                return;
            }
        }
        if (parent->children.size() == 2) {
            assert(parent == root);
            if (parent->children[0]->size() + parent->children[1]->size() < Node<KeyType, ValueType>::MAX_KEYS) {
                mergeInternal2to1(node);
            }
            return;
        }
        if (index == 0 && parent->children[index + 2]->canLend()) {
            redistributeInternal3to3(
                node,
                static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index + 1]),
                static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index + 2]),
                parent,
                index
            );
            return;
        }
        if (index == parent->children.size() - 1 && parent->children[parent->children.size() - 3]->canLend()) {
            redistributeInternal3to3(
                static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index - 2]),
                static_cast<InternalNode<KeyType, ValueType>*>(parent->children[index - 1]),
                node,
                parent,
                index - 2
            );
            return;
        }
        mergeInternal3to2(node, parentStack);
    }

    void mergeInternal2to1(InternalNode<KeyType, ValueType>* node) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::mergeInternal2to1(node=" << node << ")" << std::endl;
        }

        auto oldRoot = static_cast<InternalNode<KeyType, ValueType>*>(root);
        assert(oldRoot->children.size() == 2);
        assert(node == oldRoot->children[0] || node == oldRoot->children[1]);

        auto left = static_cast<InternalNode<KeyType, ValueType>*>(oldRoot->children[0]);
        auto right = static_cast<InternalNode<KeyType, ValueType>*>(oldRoot->children[1]);
        left->keys.push_back(oldRoot->keys[0]);
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
        left->children.insert(left->children.end(), right->children.begin(), right->children.end());

        root = left;
        oldRoot->children.clear();
        delete oldRoot;
        right->children.clear();
        delete right;
    }

    void mergeInternal3to2(
        InternalNode<KeyType, ValueType>* node,
        std::stack<std::pair<InternalNode<KeyType, ValueType>*, int>>& parentStack
    ) {
        if (Node<KeyType, ValueType>::DEBUG) {
            std::cout << "BTree::mergeInternal3to2(node=" << node << ")" << std::endl;
        }

        auto [parent, idxInParent] = parentStack.top();
        int shift;
        if (idxInParent == 0) {
            shift = 0;
        } else if (idxInParent == (int)parent->children.size() - 1) {
            shift = -2;
        } else {
            shift = -1;
        }
        auto n1 = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[idxInParent + shift]);
        auto n2 = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[idxInParent + shift + 1]);
        auto n3 = static_cast<InternalNode<KeyType, ValueType>*>(parent->children[idxInParent + shift + 2]);

        std::vector<KeyType> allK = std::move(n1->keys);
        std::vector<Node<KeyType, ValueType>*> allC = std::move(n1->children);

        auto collect = [&](InternalNode<KeyType, ValueType>* n) {
            allK.insert(allK.end(), n->keys.begin(), n->keys.end());
            allC.insert(allC.end(), n->children.begin(), n->children.end());
        };
        allK.push_back(parent->keys[idxInParent + shift]);
        collect(n2);
        allK.push_back(parent->keys[idxInParent + shift + 1]);
        collect(n3);

        size_t midKeyIdx = allK.size() / 2;
        KeyType newParentKey = allK[midKeyIdx];

        n1->keys.assign(allK.begin(), allK.begin() + midKeyIdx);
        n1->children.assign(allC.begin(), allC.begin() + midKeyIdx + 1);
        n2->keys.assign(allK.begin() + midKeyIdx + 1, allK.end());
        n2->children.assign(allC.begin() + midKeyIdx + 1, allC.end());

        parent->keys[idxInParent + shift] = newParentKey;
        parent->keys.erase(parent->keys.begin() + idxInParent + shift + 1);
        parent->children.erase(parent->children.begin() + idxInParent + shift + 2);

        n3->keys.clear();
        n3->children.clear();
        delete n3;

        if (parent != root && parent->underflow()) {
            parentStack.pop();
            handleInternalUnderflow(parent, parentStack);
        }
    }

    void printNode(std::ostream& os, Node<KeyType, ValueType>* node, int depth) const {
        if (!node) {
            return;
        }
        std::string indent(depth * 2, ' ');
        if (node->isLeaf()) {
            auto leaf = static_cast<LeafNode<KeyType, ValueType>*>(node);
            os << indent << node << " Leaf (" << node->size() << "): ";
            for (size_t i = 0; i < leaf->size(); ++i) {
                os << leaf->keys[i] << " ";
            }
            os << "\n";
        } else {
            auto in = static_cast<InternalNode<KeyType, ValueType>*>(node);
            os << indent << node << " Internal: (" << node->size() << "); ";
            for (size_t i = 0; i < in->size(); ++i) {
                os << in->keys[i] << " ";
            }
            os << "\n";
            for (auto* child : in->children) {
                printNode(os, child, depth + 1);
            }
        }
    }

public:
    bool check_integrity() const {
        if (root == nullptr) {
            return true;
        }
        auto [childMin, childMax, childValid] = check_node_integrity(root);
        return childValid;
    }

private:
    std::tuple<KeyType, KeyType, bool> check_node_integrity(Node<KeyType, ValueType>* node) const {
        if (!node) {
            return {KeyType(), KeyType(), false};
        }
        bool isRootChild =
            (node != root) &&
            (std::find(
                 static_cast<InternalNode<KeyType, ValueType>*>(root)->children.begin(),
                 static_cast<InternalNode<KeyType, ValueType>*>(root)->children.end(),
                 node
             ) != static_cast<InternalNode<KeyType, ValueType>*>(root)->children.end());
        int minAllowed = isRootChild ? (Node<KeyType, ValueType>::MAX_KEYS / 2) : Node<KeyType, ValueType>::MIN_KEYS;
        bool result = (node == root || !(node->size() < minAllowed)) && !node->overflow();
        for (size_t i = 0; i + 1 < node->size(); ++i) {
            if (node->keys[i] >= node->keys[i + 1]) {
                result = false;
            }
        }
        if (node->isLeaf()) {
            auto leaf = static_cast<LeafNode<KeyType, ValueType>*>(node);
            if (leaf->keys.empty()) {
                return {KeyType(), KeyType(), false};
            }
            return {leaf->keys.front(), leaf->keys.back(), result};
        }
        auto in = static_cast<InternalNode<KeyType, ValueType>*>(node);
        if (in->children.empty()) {
            return {KeyType(), KeyType(), false};
        }
        KeyType globalMin, globalMax;
        for (size_t i = 0; i < in->children.size(); ++i) {
            auto [childMin, childMax, childValid] = check_node_integrity(in->children[i]);
            result &= childValid;
            if (i > 0) {
                if (in->keys[i - 1] > childMin) {
                    result = false;
                }
            }
            if (i < in->size()) {
                if (childMax >= in->keys[i]) {
                    result = false;
                }
            }
            if (i == 0) {
                globalMin = childMin;
            }
            if (i == in->children.size() - 1) {
                globalMax = childMax;
            }
        }
        return {globalMin, globalMax, result};
    }
};

#endif  // QUASARDB_B_STAR_PLUS_TREE_H