// This file contains the declaration and implementation of the B*+ tree node.

#ifndef QUASARBD_B_STAR_PLUS_TREE_NODE_H
#define QUASARBD_B_STAR_PLUS_TREE_NODE_H

#include <algorithm>
#include <concepts>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>
#include "pager.h"

template<typename T>
concept BTreeKey = requires(const T a, const T b) {
    { a < b } -> std::convertible_to<bool>;
    requires std::is_trivially_copyable_v<T>; 
    requires sizeof(T) > 0;
};

template<typename T>
concept BTreeValue = requires(const T a, const T b) {
    { a < b } -> std::convertible_to<bool>;
    requires std::is_trivially_copyable_v<T>; 
    requires sizeof(T) > 0;
};

template <typename K_t, typename V_t>
class Node final {
public:
    using node_size_t = int32_t;

    static constexpr bool DEBUG = false;
    static constexpr std::string_view HEADER = "BSTARPLUSTREE_NODE";
    
    enum class NodeType {
        INTERNAL,
        LEAF,
    };

private:
    // #pragma pack(push, 1)
    struct NodeHeaderStruct {
        char header[HEADER.size() + 1];
        node_size_t node_id;
        bool is_leaf;
        node_size_t keys_size;
        bool deleted = false;
    };
    // #pragma pack(pop)

public:
    static constexpr node_size_t PAGE_SIZE = 4096;
    static constexpr uint32_t NODE_PAGES_FROM_ID = 1;

    static constexpr node_size_t KEY_SIZE = sizeof(K_t);
    static constexpr node_size_t PAGE_ID_SIZE = sizeof(node_size_t);
    static constexpr node_size_t VALUE_SIZE = sizeof(V_t);

    static constexpr node_size_t PHYS_KEYS_INTERNAL = (PAGE_SIZE - sizeof(NodeHeaderStruct) - PAGE_ID_SIZE) / (KEY_SIZE + PAGE_ID_SIZE);
    static constexpr node_size_t MAX_KEYS_INTERNAL = PHYS_KEYS_INTERNAL - 1;
    static constexpr node_size_t CHILDREN_SIZE = PHYS_KEYS_INTERNAL + 1;
    static constexpr node_size_t MAX_CHILDREN = CHILDREN_SIZE - 1;
    static constexpr node_size_t MIN_KEYS_INTERNAL = (2 * PHYS_KEYS_INTERNAL - 1) / 3;

    static constexpr node_size_t PHYS_SIZE_LEAF = (PAGE_SIZE - sizeof(NodeHeaderStruct) - PAGE_ID_SIZE) / (KEY_SIZE + VALUE_SIZE);
    static constexpr node_size_t MAX_KEYS_LEAF = PHYS_SIZE_LEAF - 1;
    static constexpr node_size_t MAX_VALUES = MAX_KEYS_LEAF;
    static constexpr node_size_t MIN_KEYS_LEAF = (2 * PHYS_SIZE_LEAF - 1) / 3;

private:
    // #pragma pack(push, 1)
    struct InternalStruct {
        NodeHeaderStruct header;
        K_t keys[PHYS_KEYS_INTERNAL];
        node_size_t children[CHILDREN_SIZE];

        InternalStruct() : header() {}
    };
    // #pragma pack(pop)

    // #pragma pack(push, 1)
    struct LeafStruct {
        NodeHeaderStruct header;
        node_size_t next_leaf;
        K_t keys[PHYS_SIZE_LEAF];
        V_t values[PHYS_SIZE_LEAF];

        LeafStruct() : header() {}
    };
    // #pragma pack(pop)

    union NodePage {
        NodeHeaderStruct header;
        InternalStruct internal_data;
        LeafStruct leaf_data;
        uint8_t raw[PAGE_SIZE];

        NodePage() : header() {}
    };

    static_assert(sizeof(NodePage) == PAGE_SIZE);

private:
    Pager* pager;
    union NodePage page_data;

public:
    explicit Node(NodeType type, Pager* pager) : pager(pager) {
        // create new node
        if (DEBUG) { std::cout << "Node::Node: type=" << static_cast<int>(type) << ", pager=" << pager << std::endl; }
        bool deleted_found = false;
        for (auto i = NODE_PAGES_FROM_ID; i < pager->get_total_pages(); ++i) {
            pager->read_page(i, reinterpret_cast<uint8_t*>(page_data.raw));
            if (page_data.header.deleted) {
                deleted_found = true;
                break;
            }
        }
        if (!deleted_found) {
            page_data.header.node_id = pager->append_new_page();
        }
        if (type == NodeType::INTERNAL) {
            page_data.header.is_leaf = false;
        } else if (type == NodeType::LEAF) {
            page_data.header.is_leaf = true;
            page_data.leaf_data.next_leaf = -1;
        } else {
            assert(false && "Unexpected NodeType");
        }
        std::memcpy(page_data.header.header, HEADER.data(), HEADER.size());
        page_data.header.header[HEADER.size()] = '\0';
        page_data.header.deleted = false;
        page_data.header.keys_size = 0;
        pager->write_page(page_data.header.node_id, page_data.raw);
    }

    explicit Node(node_size_t page_id, Pager* pager) : pager(pager) {
        // read exists node
        if (DEBUG) { std::cout << "Node::Node: page_id=" << page_id << ", pager=" << pager << std::endl; }
        pager->read_page(page_id, reinterpret_cast<uint8_t*>(page_data.raw));
        auto header = std::string_view(page_data.header.header);
        if (header != HEADER) {
            throw std::runtime_error("Headers don't match. Read: " + std::string(header) + ". Expected: " + std::string(HEADER) + ".");
        }
        if (page_id != page_data.header.node_id) {
            throw std::runtime_error("Page_id don't match. Read: " + std::to_string(page_data.header.node_id) + ". Expected: " + std::to_string(page_id) + ".");
        }
        if (page_data.header.deleted) {
            throw std::runtime_error("Read deleted Node with id " + std::to_string(page_data.header.node_id) + ".");
        }
    }

    Node(const Node& other) = delete;
    Node& operator=(const Node& other) = delete;
    Node(Node&& other) noexcept = delete;
    Node& operator=(Node&& other) noexcept = delete;

    ~Node() noexcept = default;

private:
    void increment_size() {
        if (DEBUG) { std::cout << "Node[" << id() << "]::increment_size" << std::endl; }
        ++page_data.header.keys_size;
    }

    void decrement_size() {
        if (DEBUG) { std::cout << "Node[" << id() << "]::decrement_size" << std::endl; }
        --page_data.header.keys_size;
    }

    void set_size(node_size_t new_size) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::set_size: new_size=" << new_size << std::endl; }
        page_data.header.keys_size = new_size;
    }

public:
    void set_next(node_size_t next_id) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::set_next: next_id=" << next_id << std::endl; }
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        page_data.leaf_data.next_leaf = next_id;
        save_to_disk();
    }

    void set_key(K_t key, node_size_t idx) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::set_key: key=" << key << ", idx=" << idx << std::endl; }
        if (idx >= size()) {
            throw std::out_of_range("Idx is out of range.");
        }
        if (is_leaf()) {
            page_data.leaf_data.keys[idx] = key;
        } else {
            page_data.internal_data.keys[idx] = key;
        }
        save_to_disk();
    }

    void save_to_disk() {
        if (pager) {
            pager->write_page(id(), page_data.raw);
        }
    }

    void mark_deleted() {
        if (DEBUG) { std::cout << "Node[" << id() << "]::mark_deleted" << std::endl; }
        page_data.header.deleted = true;
        save_to_disk();
    }

    bool operator==(const Node& other) const {
        return id() == other.id();
    }

    node_size_t id() const {
        return page_data.header.node_id;
    }

    bool is_leaf() const {
        return page_data.header.is_leaf;
    }

    node_size_t size() const {
        return page_data.header.keys_size;
    }

    bool empty() const {
        return size() == 0;
    }

    node_size_t next_leaf_id() const {
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        return page_data.leaf_data.next_leaf;
    }
    
    node_size_t get_next_id() {
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        return page_data.leaf_data.next_leaf;
    }

    node_size_t children_size() const {
        return size() + 1;
    }

    K_t get_key(node_size_t idx) const {
        assert(idx < size());
        if (is_leaf()) {
            return page_data.leaf_data.keys[idx];
        }
        return page_data.internal_data.keys[idx];
    }

    node_size_t get_child_id(node_size_t idx) const {
        if (is_leaf()) {
            throw std::runtime_error("It is not internal.");
        }
        assert(idx < children_size());
        return page_data.internal_data.children[idx];
    }

    bool contain_child(node_size_t child_id) const {
        node_size_t* children_ptr = page_data.internal_data.children;
        return std::find(children_ptr, children_ptr + children_size(), child_id) != children_ptr + children_size();
    }

    V_t get_value(node_size_t idx) const {
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        assert(idx < size());
        return page_data.leaf_data.values[idx];
    }

    bool can_lend(bool one_of_two_root_child = false) const {
        if (one_of_two_root_child) {
            return page_data.header.is_leaf ? size() > MAX_KEYS_LEAF / 2 : size() > MAX_KEYS_INTERNAL / 2;
        }
        return page_data.header.is_leaf ? size() > MIN_KEYS_LEAF : size() > MIN_KEYS_INTERNAL;
    }

    bool can_take() const {
        return page_data.header.is_leaf ? size() < MAX_KEYS_LEAF : size() < MAX_KEYS_INTERNAL;
    }

    bool underflow() const {
        return is_leaf() ? size() < MIN_KEYS_LEAF : size() < MIN_KEYS_INTERNAL;
    }

    bool overflow() const {
        return is_leaf() ? size() > MAX_KEYS_LEAF : size() > MAX_KEYS_INTERNAL;
    }

    node_size_t find_child_idx(K_t key) const {
        if (is_leaf()) {
            throw std::runtime_error("It is not internal.");
        }
        const K_t* key_ptr = page_data.internal_data.keys;
        return std::upper_bound(key_ptr, key_ptr + size(), key) - key_ptr;
    }

private:
    void move_right_internal(node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::move_right_internal: count=" << count << std::endl; }
        if (children_size() + count > MAX_CHILDREN) {
            throw std::runtime_error("children_size() + count > MAX_CHILDREN");
        }
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        std::move_backward(
            key_ptr,
            key_ptr + size(),
            key_ptr + size() + count
        );
        std::move_backward(
            children_ptr,
            children_ptr + children_size(),
            children_ptr + children_size() + count
        );
        set_size(size() + count);
    }

    K_t move_left_internal(node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::move_left_internal: count=" << count << std::endl; }
        if (count == 0) {
            throw std::runtime_error("count == 0");
        }
        if (count > children_size()) {
            throw std::runtime_error("count > children_size()");
        }
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        K_t mid_key = key_ptr[count - 1];
        std::move(
            key_ptr + count,
            key_ptr + size(),
            key_ptr
        );
        std::move(
            children_ptr + count,
            children_ptr + children_size(),
            children_ptr
        );
        set_size(size() - count);
        return mid_key;
    }

public:
    K_t send_to_right_internal(Node& right, node_size_t children_count, K_t mid_key) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::send_to_right_internal: right=" << right.id() << ", children_count=" << children_count << ", mid_key=" << mid_key << std::endl; }
        if (is_leaf() || right.is_leaf()) {
            throw std::runtime_error("It is not internal.");
        }
        node_size_t send_keys_count = children_count - 1;
        right.move_right_internal(children_count);
        right.page_data.internal_data.keys[send_keys_count] = mid_key;
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        std::move(
            key_ptr + size() - send_keys_count,
            key_ptr + size(),
            right.page_data.internal_data.keys
        );
        std::move(
            children_ptr + children_size() - children_count,
            children_ptr + children_size(),
            right.page_data.internal_data.children
        );
        set_size(size() - send_keys_count);
        right.save_to_disk();
        save_to_disk();
        return key_ptr[size()];
    }

    K_t take_from_right_internal(Node& right, node_size_t children_count, K_t mid_key) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::take_from_right_internal: right=" << right.id() << ", children_count=" << children_count << ", mid_key=" << mid_key << std::endl; }
        if (is_leaf() || right.is_leaf()) {
            throw std::runtime_error("It is not internal.");
        }
        if (children_size() + children_count > MAX_CHILDREN) {
            throw std::runtime_error("children_size() + children_count > MAX_CHILDREN");
        }
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        auto old_size = size();
        key_ptr[old_size] = mid_key;
        node_size_t take_keys_count = children_count - 1;
        std::move(
            right.page_data.internal_data.keys,
            right.page_data.internal_data.keys + take_keys_count,
            key_ptr + old_size + 1
        );
        std::move(
            right.page_data.internal_data.children,
            right.page_data.internal_data.children + children_count,
            children_ptr + old_size + 1
        );
        K_t new_mid_key = right.move_left_internal(children_count);
        set_size(size() + take_keys_count);
        right.save_to_disk();
        save_to_disk();
        return new_mid_key;
    }

private:
    void move_right_leaf(node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::move_right_leaf: count=" << count << std::endl; }
        if (size() + count > MAX_KEYS_LEAF) {
            throw std::runtime_error("size() + count > MAX_KEYS_LEAF");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        std::move_backward(
            key_ptr,
            key_ptr + size(),
            key_ptr + size() + count
        );
        std::move_backward(
            values_ptr,
            values_ptr + size(),
            values_ptr + size() + count
        );
        set_size(size() + count);
    }

    K_t move_left_leaf(node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::move_left_leaf: count=" << count << std::endl; }
        if (count > size()) {
            throw std::runtime_error("count > size()");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        K_t new_mid_key = key_ptr[count];
        if (count == 0) {
            return new_mid_key;
        }
        std::move(
            key_ptr + count,
            key_ptr + size(),
            key_ptr
        );
        std::move(
            values_ptr + count,
            values_ptr + size(),
            values_ptr
        );
        set_size(size() - count);
        return new_mid_key;
    }

public:
    K_t send_to_right_leaf(Node& right, node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::send_to_right_leaf: right=" << right.id() << ", count=" << count << std::endl; }
        if (!is_leaf() || !right.is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        right.move_right_leaf(count);
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        std::move(
            key_ptr + size() - count,
            key_ptr + size(),
            right.page_data.leaf_data.keys
        );
        std::move(
            values_ptr + size() - count,
            values_ptr + size(),
            right.page_data.leaf_data.values
        );
        set_size(size() - count);
        right.save_to_disk();
        save_to_disk();
        return right.page_data.leaf_data.keys[0];
    }

    K_t take_from_right_leaf(Node& right, node_size_t count) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::take_from_right_leaf: right=" << right.id() << ", count=" << count << std::endl; }
        if (!is_leaf() || !right.is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        if (size() + count > MAX_KEYS_LEAF) {
            throw std::runtime_error("size() + count > MAX_KEYS_LEAF");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        std::move(
            right.page_data.leaf_data.keys,
            right.page_data.leaf_data.keys + count,
            key_ptr + size()
        );
        std::move(
            right.page_data.leaf_data.values,
            right.page_data.leaf_data.values + count,
            values_ptr + size()
        );
        set_size(size() + count);
        K_t new_mid_key = right.move_left_leaf(count);
        right.save_to_disk();
        save_to_disk();
        return new_mid_key;
    }

public:
    void push_back_to_internal(node_size_t child_id) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::push_back_to_internal: child_id=" << child_id << std::endl; }
        if (size() != 0) {
            throw std::runtime_error("For not empty internal nodes use insert_in_internal(key, right_child_id)");
        }
        page_data.internal_data.children[0] = child_id;
        save_to_disk();
    }

    bool insert_in_internal(K_t key, node_size_t right_child_id) {
        if (DEBUG) {
            std::cout
                << "Node[" << id() << "]::insert_in_internal(key=" << key << ", right_child_id=" << right_child_id << ")"
                << std::endl;
        }
        if (is_leaf()) {
            throw std::runtime_error("It is not internal.");
        }
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        auto* it = std::lower_bound(key_ptr, key_ptr + size(), key);
        int index = std::distance(key_ptr, it);
        assert(index < PHYS_KEYS_INTERNAL);
        std::move_backward(
            key_ptr + index,
            key_ptr + size(),
            key_ptr + size() + 1
        );
        std::move_backward(
            children_ptr + index + 1,
            children_ptr + children_size(),
            children_ptr + children_size() + 1
        );
        key_ptr[index] = key;
        children_ptr[index + 1] = right_child_id;
        increment_size();
        save_to_disk();
        return overflow();
    }

    bool insert_in_leaf(K_t key, V_t value) {
        if (DEBUG) {
            std::cout << "Node[" << id() << "]::insert_in_leaf(key=" << key << ", value=" << value << ")" << std::endl;
        }
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        auto* it = std::lower_bound(key_ptr, key_ptr + size(), key);
        int index = std::distance(key_ptr, it);
        if (index < size() && *it == key) {
            return false;
        }
        std::move_backward(
            key_ptr + index, 
            key_ptr + size(), 
            key_ptr + size() + 1
        );
        std::move_backward(
            values_ptr + index, 
            values_ptr + size(), 
            values_ptr + size() + 1
        );
        key_ptr[index] = key;
        values_ptr[index] = value;
        increment_size();
        save_to_disk();
        return true;
    }

    bool remove_from_leaf(K_t key) {
        if (DEBUG) {
            std::cout << "Node[" << id() << "]::remove_from_leaf(key=" << key << ")" << std::endl;
        }
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        V_t* values_ptr = page_data.leaf_data.values;
        auto* it = std::lower_bound(key_ptr, key_ptr + size(), key);
        int index = std::distance(key_ptr, it);
        if (it == key_ptr + size() || *it != key) {
            return false;
        }
        std::move(
            key_ptr + index + 1, 
            key_ptr + size(), 
            key_ptr + index
        );
        std::move(
            values_ptr + index + 1, 
            values_ptr + size(), 
            values_ptr + index
        );
        decrement_size();
        save_to_disk();
        return true;
    }

    void remove_from_internal(int key_idx) {
        if (DEBUG) { std::cout << "Node[" << id() << "]::remove_from_internal(key_idx=" << key_idx << ")" << std::endl; }
        K_t* key_ptr = page_data.internal_data.keys;
        node_size_t* children_ptr = page_data.internal_data.children;
        std::move(
            key_ptr + key_idx + 1,
            key_ptr + size(),
            key_ptr + key_idx
        );
        std::move(children_ptr + key_idx + 2,
            children_ptr + children_size(),
            children_ptr + key_idx + 1
        );
        decrement_size();
        save_to_disk();
    }

    std::optional<V_t> search(K_t key) const {
        if (DEBUG) {
            std::cout << "Node[" << id() << "]::search(key=" << key << ")" << std::endl;
        }
        if (!is_leaf()) {
            throw std::runtime_error("It is not leaf.");
        }
        K_t* key_ptr = page_data.leaf_data.keys;
        auto* it = std::lower_bound(key_ptr, key_ptr + size(), key);
        if (it == key_ptr + size() || *it != key) {
            return std::nullopt;
        }
        node_size_t idx = std::distance(key_ptr, it);
        return page_data.leaf_data.values[idx];
    }
};

#endif  // QUASARBD_B_STAR_PLUS_TREE_NODE_H