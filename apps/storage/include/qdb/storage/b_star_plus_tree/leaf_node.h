#ifndef QUASARDB_B_STAR_PLUS_TREE_LEAF_NODE_HPP
#define QUASARDB_B_STAR_PLUS_TREE_LEAF_NODE_HPP

#include "node.h"

template <typename KeyType, typename ValueType>
class LeafNode : public Node<KeyType, ValueType> {

public:

    using Node<KeyType, ValueType>::keys;
    std::vector<ValueType> values;
    LeafNode<KeyType, ValueType>* next;
    LeafNode<KeyType, ValueType>* prev;

    LeafNode() : next(nullptr), prev(nullptr) {
        keys.reserve(Node<KeyType, ValueType>::MAX_KEYS + 1);
        values.reserve(Node<KeyType, ValueType>::MAX_KEYS + 1);
    }

    bool isLeaf() const override { return true; }

    bool insert(const KeyType& key, const ValueType& value) {
        if (Node<KeyType, ValueType>::DEBUG) std::cout << "LeafNode<KeyType, ValueType>::insert(key=" << key << ", value=" << value << ")" << std::endl;
        auto it = std::lower_bound(keys.begin(), keys.end(), key);
        int pos = it - keys.begin();
        keys.insert(it, key);
        values.insert(values.begin() + pos, value);
        return keys.size() > Node<KeyType, ValueType>::MAX_KEYS;
    }

    bool remove(const KeyType& key) {
        if (Node<KeyType, ValueType>::DEBUG) std::cout << "LeafNode<KeyType, ValueType>::remove(key=" << key << ")" << std::endl;
        auto it = std::lower_bound(keys.begin(), keys.end(), key);
        int pos = it - keys.begin();
        if (it == keys.end() || *it != key)
            return false;
        keys.erase(it);
        values.erase(values.begin() + pos);
        return true;
    }

    std::optional<ValueType> search(const KeyType& key) const {
        if (Node<KeyType, ValueType>::DEBUG) std::cout << "LeafNode<KeyType, ValueType>::search(key=" << key << ")" << std::endl;
        auto it = std::lower_bound(keys.begin(), keys.end(), key);
        if (it == keys.end() || *it != key)
            return std::nullopt;
        int idx = it - keys.begin();
        return values[idx];
    }

};

#endif  // QUASARDB_B_STAR_PLUS_TREE_LEAF_NODE_HPP