#ifndef QUASARBD_B_STAR_PLUS_TREE_NODE_HPP
#define QUASARBD_B_STAR_PLUS_TREE_NODE_HPP

#include <vector>

template <typename KeyType, typename ValueType>
class Node {
    
public:
    static constexpr bool DEBUG = false;

    static constexpr int ORDER = 6;
    static constexpr int MAX_KEYS = ORDER - 1;
    static constexpr int MIN_KEYS = (2 * ORDER - 1) / 3;

    std::vector<KeyType> keys;          // отсортированные (int или string в контексте БД)
    
    virtual ~Node() = default;
    virtual bool isLeaf() const = 0;

    int size() const {
        return static_cast<int>(keys.size());
    }

    KeyType getKey(int idx) const {
        return keys[idx];
    }

    bool canLend() const {
        // Проверка, можно ли отдать часть ключей соседу (для перераспределения)
        return keys.size() > MIN_KEYS;
    }

    bool underflow() const {
        // Проверка, не нарушен ли минимум
        return keys.size() < MIN_KEYS;
    }

    bool overflow() const {
        return keys.size() > MAX_KEYS;
    }

};

#endif  // QUASARBD_B_STAR_PLUS_TREE_NODE_HPP