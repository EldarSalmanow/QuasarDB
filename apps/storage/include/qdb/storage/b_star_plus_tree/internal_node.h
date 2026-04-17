#ifndef QUASARDB_B_STAR_PLUS_TREE_IN_NODE_HPP
#define QUASARDB_B_STAR_PLUS_TREE_IN_NODE_HPP

#include "node.h"

template <typename KeyType, typename ValueType>
class InternalNode : public Node<KeyType, ValueType> {

public:

    using Node<KeyType, ValueType>::keys;
    std::vector<Node<KeyType, ValueType>*> children;

    InternalNode() {
        keys.reserve(Node<KeyType, ValueType>::MAX_KEYS + 1);
        children.reserve(Node<KeyType, ValueType>::MAX_KEYS + 2);
    }

    bool isLeaf() const override { return false; }

    bool insertKey(const KeyType& key, Node<KeyType, ValueType>* rightChild) {
        if (Node<KeyType, ValueType>::DEBUG) std::cout << "InternalNode<KeyType, ValueType>::insert(key=" << key << ", r_child=" << rightChild << ")" << std::endl;
        auto it = std::lower_bound(keys.begin(), keys.end(), key);
        int pos = it - keys.begin();
        keys.insert(it, key);
        children.insert(children.begin() + pos + 1, rightChild);
        return keys.size() > Node<KeyType, ValueType>::MAX_KEYS;
    }

};

#endif  // QUASARDB_B_STAR_PLUS_TREE_IN_NODE_HPP