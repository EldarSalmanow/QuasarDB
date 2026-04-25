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
#include <span>
#include <concepts>
#include <string_view>
#include <cstdint>
#include <cstring>
#include "node.h"
#include "pager.h"

template<BTreeKey K_t, BTreeValue V_t>
class BStarPlusTree final {
    using NodeType = Node<K_t, V_t>::NodeType;
    using node_size_t = Node<K_t, V_t>::node_size_t;

private:
    static constexpr bool DEBUG = Node<K_t, V_t>::DEBUG;

    static constexpr node_size_t METADATA_PAGE_ID = 0;
    static constexpr std::string_view HEADER = "BSTARPLUSTREE";

    // #pragma pack(push, 1)
    struct MetadataStruct {
        char header[HEADER.size() + 1];
        node_size_t root_id;
        node_size_t first_leaf_id;

        MetadataStruct() : root_id(-1), first_leaf_id(-1) {
            std::memcpy(header, HEADER.data(), HEADER.size());
            header[HEADER.size()] = '\0';
        }
    };
    // #pragma pack(pop)

    union MetadataPage {
        MetadataStruct metadata_struct;
        uint8_t raw[Node<K_t, V_t>::PAGE_SIZE];

        MetadataPage() : metadata_struct() {}
    };

    std::string path;
    Pager pager;
    union MetadataPage metadata;

private:
    void read_metadata() {
        if (DEBUG) { std::cout << "BStarPlusTree::read_metadata" << std::endl; }
        pager.read_page(METADATA_PAGE_ID, reinterpret_cast<uint8_t*>(metadata.raw));
        auto header = std::string_view(metadata.metadata_struct.header);
        if (header != HEADER) {
            throw std::runtime_error("Headers don't match. Read: " + std::string(header) + ". Expected: " + std::string(HEADER) + ".");
        }
    }

    void write_metadata() {
        if (DEBUG) { std::cout << "BStarPlusTree::write_metadata" << std::endl; }
        pager.write_page(METADATA_PAGE_ID, reinterpret_cast<uint8_t*>(metadata.raw));
    }

public:
    BStarPlusTree(const std::string& path)
        : path(path), pager(path, Node<K_t, V_t>::PAGE_SIZE)
    {
        if (DEBUG) { std::cout << "BStarPlusTree::BStarPlusTree: path=" << path << std::endl; }
        if (!pager.page_exists(METADATA_PAGE_ID)) {
            if (DEBUG) { std::cout << "!pager.page_exists(METADATA_PAGE_ID)" << std::endl; }
            uint32_t metadata_page_id = pager.append_new_page();
            assert(metadata_page_id == METADATA_PAGE_ID);
            set_root_id(-1);
            set_first_leaf_id(-1);
            write_metadata();
        } else {
            if (DEBUG) { std::cout << "pager.page_exists(METADATA_PAGE_ID)" << std::endl; }
            read_metadata();
        }
    }

    BStarPlusTree(const BStarPlusTree&) = delete;
    BStarPlusTree& operator=(const BStarPlusTree&) = delete;
    BStarPlusTree(BStarPlusTree&&) noexcept = delete;
    BStarPlusTree& operator=(BStarPlusTree&&) noexcept = delete;

    ~BStarPlusTree() noexcept = default;

    node_size_t root_id() {
        return metadata.metadata_struct.root_id;
    }

    void set_root_id(node_size_t id) {
        metadata.metadata_struct.root_id = id;
        write_metadata();
    }

    node_size_t first_leaf_id() {
        return metadata.metadata_struct.first_leaf_id;
    }

    void set_first_leaf_id(node_size_t id) {
        metadata.metadata_struct.first_leaf_id = id;
        write_metadata();
    }

    std::optional<V_t> search(K_t key) {
        if (DEBUG) {
            std::cout << "BStarPlusTree::search: key=" << key << std::endl;
        }
        std::stack<std::pair<node_size_t, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        if (!leaf) {
            return std::nullopt;
        }
        return leaf->search(key);
    }

    void insert(K_t key, V_t value) {
        if (DEBUG) {
            std::cout << "BStarPlusTree::insert: key=" << key << ", value=" << value << std::endl;
        }
        if (root_id() == -1) {
            auto root = std::make_unique<Node<K_t, V_t>>(NodeType::LEAF, &pager);
            root->insert_in_leaf(key, value);
            set_first_leaf_id(root->id());
            set_root_id(root->id());
            write_metadata();
            return;
        }
        std::stack<std::pair<node_size_t, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        assert(leaf);
        bool key_not_existed = leaf->insert_in_leaf(key, value);
        if (!key_not_existed) {
            throw std::runtime_error("Dublicated key inserting.");
        }
        if (leaf->overflow()) {
            handleLeafOverflow(*leaf, parentStack);
        }
    }

    bool remove(K_t key) {
        if (DEBUG) {
            std::cout << "BStarPlusTree::remove: key=" << key << std::endl;
        }
        std::stack<std::pair<node_size_t, int>> parentStack;
        auto leaf = findLeaf(key, parentStack);
        if (!leaf) {
            return false;
        }
        bool removed = leaf->remove_from_leaf(key);
        if (!removed) {
            return false;
        }
        if (leaf->id() == root_id()) {
            if (leaf->empty()) {
                leaf->mark_deleted();
                leaf->save_to_disk();
                set_root_id(-1);
                set_first_leaf_id(-1);
            }
            return true;
        }
        if (leaf->underflow()) {
            handleLeafUnderflow(*leaf, parentStack);
        }
        return true;
    }

    std::vector<std::pair<K_t, V_t>> rangeSearch(const K_t& low, const K_t& high) {
        if (DEBUG) {
            std::cout << "BStarPlusTree::rangeSearch(low=" << low << ", high=" << high << ")" << std::endl;
        }
        std::vector<std::pair<K_t, V_t>> result;
        std::stack<std::pair<node_size_t, int>> parentStack;
        auto leaf = findLeaf(low, parentStack);
        for (; leaf; leaf = make_unique<Node<K_t, V_t>>(leaf->get_next_id(), &pager)) {
            for (size_t i = 0; i < leaf->size(); ++i) {
                if (high < leaf->get_key(i)) {
                    return result;
                }
                if (low <= leaf->get_key(i)) {
                    result.emplace_back(leaf->get_key(i), leaf->get_value(i));
                }
            }
        }
        return result;
    }

    void print(std::ostream& os = std::cout) {
        printNode(os, root_id(), 0);
    }

private:
    void clear() {
        if (DEBUG) {
            std::cout << "BTree::clear" << std::endl;
        }
        pager.truncate(1);
        set_root_id(-1);
        set_first_leaf_id(-1);
        write_metadata();
    }

    std::unique_ptr<Node<K_t, V_t>> findLeaf(K_t key, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::findLeaf: key=" << key << std::endl;
        }
        auto cur = std::make_unique<Node<K_t, V_t>>(root_id(), &pager);
        while (!cur->is_leaf()) {
            node_size_t idx = cur->find_child_idx(key);
            parentStack.push({cur->id(), idx});
            cur = std::make_unique<Node<K_t, V_t>>(cur->get_child_id(idx), &pager);
        }
        return cur;
    }

    void handleLeafOverflow(Node<K_t, V_t>& leaf, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::handleLeafOverflow: leaf=" << leaf.id() << std::endl;
        }
        if (leaf.id() == root_id()) {
            splitLeaf1to2(leaf);
            return;
        }
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);
        std::unique_ptr<Node<K_t, V_t>> left;
        if (idx_in_parent > 0) {
            left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 1), &pager);
            if (left->can_take()) {
                redistributeLeaves2to2(*left, leaf, *parent, idx_in_parent - 1);
                return;
            }
        }
        std::unique_ptr<Node<K_t, V_t>> right;
        if (idx_in_parent + 1 < parent->children_size()) {
            right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 1), &pager);
            if (right->can_take()) {
                redistributeLeaves2to2(leaf, *right, *parent, idx_in_parent);
                return;
            }
        }
        if (left) {
            splitLeaves2to3(*left, leaf, parentStack, idx_in_parent - 1);
            return;
        }
        if (right) {
            splitLeaves2to3(leaf, *right, parentStack, idx_in_parent);
            return;
        }
        throw std::runtime_error("This code must be unreachable.");
    }

    void splitLeaf1to2(Node<K_t, V_t>& node) {
        if (DEBUG) {
            std::cout << "BTree::splitLeaf1to2: node=" << node.id() << std::endl;
        }
        auto new_root = std::make_unique<Node<K_t, V_t>>(NodeType::INTERNAL, &pager);
        set_root_id(new_root->id());
        new_root->push_back_to_internal(node.id());
        auto right_leaf = std::make_unique<Node<K_t, V_t>>(NodeType::LEAF, &pager);

        new_root->insert_in_internal(K_t(), right_leaf->id());
        redistributeLeaves2to2(node, *right_leaf, *new_root, 0);
        node.set_next(right_leaf->id());
    }

    void redistributeLeaves2to2(Node<K_t, V_t>& left, Node<K_t, V_t>& right, Node<K_t, V_t>& parent, node_size_t left_idx) {
        if (DEBUG) {
            std::cout
                << "BTree::redistributeLeaves2to2(left=" << left.id() << ", right=" << right.id() << ", parent=" << parent.id()
                << ", left_idx=" << left_idx << ")" << std::endl;
        }

        auto total = left.size() + right.size();
        auto s1 = total / 2;

        if (left.size() != s1) {
            auto new_mid_key = left.size() > s1 ?
                left.send_to_right_leaf(right, left.size() - s1) :
                left.take_from_right_leaf(right, s1 - left.size());
            parent.set_key(new_mid_key, left_idx);
        }
    }

    void redistributeLeaves3to3(Node<K_t, V_t>& left, Node<K_t, V_t>& middle, Node<K_t, V_t>& right, Node<K_t, V_t>& parent, node_size_t left_idx) {
        if (DEBUG) {
            std::cout
                << "BTree::redistributeLeaves3to3(left=" << left.id() << ", middle=" << middle.id() << ", right=" << right.id()
                << ", parent=" << parent.id() << ", left_idx=" << left_idx << ")" << std::endl;
        }

        auto total = left.size() + middle.size() + right.size();
        auto s1 = total / 3;
        auto s2 = (total - s1) / 2;
        auto s3 = total - s1 - s2;

        K_t mid1;
        K_t mid2;

        if (left.underflow()) {
            assert(left.underflow() && (middle.size() == Node<K_t, V_t>::MIN_KEYS_LEAF) && right.can_lend());
            mid1 = left.take_from_right_leaf(middle, s1 - left.size());
            mid2 = middle.take_from_right_leaf(right, s2 - middle.size());
        } else if (right.underflow()) {
            assert(left.can_lend() && (middle.size() == Node<K_t, V_t>::MIN_KEYS_LEAF) && right.underflow());
            mid2 = middle.send_to_right_leaf(right, s3 - right.size());
            mid1 = left.send_to_right_leaf(middle, s2 - middle.size());
        } else {
            assert(false && "Wrong using redistributeLeaves3to3");
        }
        parent.set_key(mid1, left_idx);
        parent.set_key(mid2, left_idx + 1);
    }

    void splitLeaves2to3(Node<K_t, V_t>& left, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack, node_size_t left_idx) {
        if (DEBUG) {
            std::cout << "BTree::splitLeaves2to3(left=" << left.id() << ", right=" << right.id() << ", left_idx=" << left_idx << ")" << std::endl;
        }

        auto n3 = std::make_unique<Node<K_t, V_t>>(NodeType::LEAF, &pager);
        
        auto total = left.size() + right.size();
        auto s1 = total / 3;
        auto s2 = (total - s1) / 2;
        auto s3 = total - s1 - s2;
        
        auto mid_2 = right.send_to_right_leaf(*n3, s3);
        auto mid_1 = left.send_to_right_leaf(right, s2 - right.size());
        
        n3->set_next(right.get_next_id());
        right.set_next(n3->id());
        
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);
        parent->set_key(mid_1, left_idx);
        parent->insert_in_internal(mid_2, n3->id());

        if (parent->overflow()) {
            parentStack.pop();
            handleInternalOverflow(*parent, parentStack);
        }
    }

    void handleInternalOverflow(Node<K_t, V_t>& node, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::handleInternalOverflow(node=" << node.id() << ")" << std::endl;
        }
        if (node.id() == root_id()) {
            splitInternal1to2(node);
            return;
        }
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);
        std::unique_ptr<Node<K_t, V_t>> left;
        if (idx_in_parent > 0) {
            left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 1), &pager);
            if (left->can_take()) {
                redistributeInternal2to2(*left, node, *parent, idx_in_parent - 1);
                return;
            }
        }
        std::unique_ptr<Node<K_t, V_t>> right;
        if (idx_in_parent < (int)parent->children_size() - 1) {
            right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 1), &pager);
            if (right->can_take()) {
                redistributeInternal2to2(node, *right, *parent, idx_in_parent);
                return;
            }
        }
        if (left) {
            splitInternal2to3(*left, node, parentStack, idx_in_parent - 1);
            return;
        }
        if (right) {
            splitInternal2to3(node, *right, parentStack, idx_in_parent);
            return;
        }
        throw std::runtime_error("Some wrong.");
    }

    void redistributeInternal2to2(Node<K_t, V_t>& left, Node<K_t, V_t>& right, Node<K_t, V_t>& parent, node_size_t left_idx) {
        if (DEBUG) {
            std::cout
                << "BTree::redistributeInternal2to2(left=" << left.id() << ", right=" << right.id() << ", parent=" << parent.id()
                << ", left_idx=" << left_idx << ")" << std::endl;
        }
        
        auto total = left.size() + right.size();
        auto s1 = total / 2;

        auto old_mid_key = parent.get_key(left_idx);

        if (left.size() != s1) {
            auto new_mid_key = left.size() > s1 ?
                left.send_to_right_internal(right, left.size() - s1, old_mid_key) :
                left.take_from_right_internal(right, s1 - left.size(), old_mid_key);
            parent.set_key(new_mid_key, left_idx);
        }
    }

    void redistributeInternal3to3(Node<K_t, V_t>& left, Node<K_t, V_t>& middle, Node<K_t, V_t>& right, Node<K_t, V_t>& parent, node_size_t left_idx) {
        if (DEBUG) {
            std::cout
                << "BTree::redistributeInternal3to3(left=" << left.id() << ", middle=" << middle.id() << ", right=" << right.id()
                << ", parent=" << parent.id() << ", left_idx=" << left_idx << ")" << std::endl;
        }

        auto total = left.children_size() + middle.children_size() + right.children_size();
        auto s1 = total / 3;
        auto s2 = (total - s1) / 2;
        auto s3 = total - s1 - s2;

        auto old_mid_1 = parent.get_key(left_idx);
        auto old_mid_2 = parent.get_key(left_idx + 1);
        K_t mid_1;
        K_t mid_2;

        if (left.underflow()) {
            assert(left.underflow() && (middle.size() == Node<K_t, V_t>::MIN_KEYS_INTERNAL) && right.can_lend());
            mid_1 = left.take_from_right_internal(middle, s1 - left.children_size(), old_mid_1);
            mid_2 = middle.take_from_right_internal(right, s2 - middle.children_size(), old_mid_2);
        } else if (right.underflow()) {
            assert(left.can_lend() && (middle.size() == Node<K_t, V_t>::MIN_KEYS_INTERNAL) && right.underflow());
            mid_2 = middle.send_to_right_internal(right, s3 - right.children_size(), old_mid_2);
            mid_1 = left.send_to_right_internal(middle, s2 - middle.children_size(), old_mid_1);
        } else {
            assert(false && "Wrong using redistributeInternal3to3");
        }
        parent.set_key(mid_1, left_idx);
        parent.set_key(mid_2, left_idx + 1);
    }

    void splitInternal2to3(Node<K_t, V_t>& left, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack, node_size_t left_idx) {
        if (DEBUG) {
            std::cout << "BTree::splitInternal2to3(left=" << left.id() << ", right=" << right.id() << ", left_idx=" << left_idx << ")" << std::endl;
        }

        auto total = left.children_size() + right.children_size();
        auto s1 = total / 3;
        auto s2 = (total - s1) / 2;
        auto s3 = total - s1 - s2;
        
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);

        auto old_mid_1 = parent->get_key(left_idx);
        auto old_mid_2 = parent->get_key(left_idx + 1);
        
        auto n3 = std::make_unique<Node<K_t, V_t>>(NodeType::INTERNAL, &pager);
        auto mid_2 = right.send_to_right_internal(*n3, s3, old_mid_2);
        auto mid_1 = left.send_to_right_internal(right, s2 - right.children_size(), old_mid_1);
        
        parent->set_key(mid_1, left_idx);
        parent->insert_in_internal(mid_2, n3->id());

        if (parent->overflow()) {
            parentStack.pop();
            handleInternalOverflow(*parent, parentStack);
        }
    }

    void splitInternal1to2(Node<K_t, V_t>& node) {
        if (DEBUG) {
            std::cout << "BTree::splitInternal1to2(node=" << node.id() << ")" << std::endl;
        }
        assert(node.id() == root_id());

        auto new_root = std::make_unique<Node<K_t, V_t>>(NodeType::INTERNAL, &pager);
        set_root_id(new_root->id());
        new_root->push_back_to_internal(node.id());

        auto right = std::make_unique<Node<K_t, V_t>>(NodeType::INTERNAL, &pager);
        new_root->insert_in_internal(K_t(), right->id());
        redistributeInternal2to2(node, *right, *new_root, 0);
    }

    void handleLeafUnderflow(Node<K_t, V_t>& leaf, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::handleLeafUnderflow(leaf=" << leaf.id() << ")" << std::endl;
        }
        // if (parentStack.empty()) {
        //     return;
        // }
        assert(leaf.id() != root_id());

        std::unique_ptr<Node<K_t, V_t>> left_left, left, right, right_right;
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);

        if (idx_in_parent > 0) {
            left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 1), &pager);
            if (left->can_lend(parent->children_size() == 2)) {
                redistributeLeaves2to2(*left, leaf, *parent, idx_in_parent - 1);
                return;
            }
        }
        if (idx_in_parent + 1 < parent->children_size()) {
            right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 1), &pager);
            if (right->can_lend(parent->children_size() == 2)) {
                redistributeLeaves2to2(leaf, *right, *parent, idx_in_parent);
                return;
            }
        }
        if (parent->children_size() == 2) {
            if (right) {
                assert((leaf.size() + right->size() <= Node<K_t, V_t>::MAX_KEYS_LEAF));
                mergeLeaves2to1(leaf, *right, parentStack);
            } else if (left) {
                assert((left->size() + leaf.size() <= Node<K_t, V_t>::MAX_KEYS_LEAF));
                mergeLeaves2to1(*left, leaf, parentStack);
            } else {
                assert(false && "This code must be unreachable.");
            }
            return;
        }
        if (idx_in_parent == 0) {
            right_right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 2), &pager);
            if (right_right->can_lend()) {
                redistributeLeaves3to3(leaf, *right, *right_right, *parent, idx_in_parent);
            } else {
                mergeLeaves3to2(leaf, *right, *right_right, parentStack, idx_in_parent);
            }
            return;
        }
        if (idx_in_parent == parent->children_size() - 1) {
            left_left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 2), &pager);
            if (left_left->can_lend()) {
                redistributeLeaves3to3(*left_left, *left, leaf, *parent, idx_in_parent - 2);
            } else {
                mergeLeaves3to2(*left_left, *left, leaf, parentStack, idx_in_parent - 2);
            }
            return;
        }
        mergeLeaves3to2(*left, leaf, *right, parentStack, idx_in_parent - 1);
    }

    void mergeLeaves3to2(Node<K_t, V_t>& left, Node<K_t, V_t>& middle, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack, node_size_t left_idx) {
        if (DEBUG) {
            std::cout << "BTree::mergeLeaves3to2(left=" << left.id() << ", middle=" << middle.id() << ", right=" << right.id() << ")" << std::endl;
        }
        
        auto total = left.size() + middle.size() + right.size();
        auto s1 = total / 2;
        auto s2 = total - s1;
        
        auto new_mid = left.take_from_right_leaf(middle, s1 - left.size());
        middle.take_from_right_leaf(right, s2 - middle.size());
        middle.set_next(right.get_next_id());
        right.mark_deleted();
        
        assert(left.size() == s1);
        assert(middle.size() == s2);
        assert(right.size() == 0);
        
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);
        parent->set_key(new_mid, left_idx);
        parent->remove_from_internal(left_idx + 1);

        if (parent->id() != root_id() && parent->underflow()) {
            parentStack.pop();
            handleInternalUnderflow(*parent, parentStack);
        }
    }

    void mergeLeaves2to1(Node<K_t, V_t>& left, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::mergeLeaves2to1(left=" << left.id() << "right=" << right.id() << ")" << std::endl;
        }

        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);

        assert(parent->id() == root_id() && parent->children_size() == 2);
        assert(right.get_next_id() == -1);
        
        left.take_from_right_leaf(right, right.size());
        left.set_next(-1);
        set_root_id(left.id());
        right.mark_deleted();
        parent->mark_deleted();
    }

    void handleInternalUnderflow(Node<K_t, V_t>& node, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::handleInternalUnderflow(node=" << node.id() << ")" << std::endl;
        }

        std::unique_ptr<Node<K_t, V_t>> left_left, left, right, right_right;
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);

        if (idx_in_parent > 0) {
            left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 1), &pager);
            if (left->can_lend(parent->children_size() == 2)) {
                redistributeInternal2to2(*left, node, *parent, idx_in_parent - 1);
                return;
            }
        }
        if (idx_in_parent + 1 < parent->children_size()) {
            right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 1), &pager);
            if (right->can_lend(parent->children_size() == 2)) {
                redistributeInternal2to2(node, *right, *parent, idx_in_parent);
                return;
            }
        }
        if (parent->children_size() == 2) {
            if (right) {
                assert((node.size() + right->size() <= Node<K_t, V_t>::MAX_KEYS_INTERNAL));
                mergeInternal2to1(node, *right, parentStack);
            } else if (left) {
                assert((left->size() + node.size() <= Node<K_t, V_t>::MAX_KEYS_INTERNAL));
                mergeInternal2to1(*left, node, parentStack);
            } else {
                assert(false && "This code must be unreachable.");
            }
            return;
        }
        if (idx_in_parent == 0) {
            right_right = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent + 2), &pager);
            if (right_right->can_lend()) {
                redistributeInternal3to3(node, *right, *right_right, *parent, idx_in_parent);
            } else {
                mergeInternal3to2(node, *right, *right_right, parentStack, idx_in_parent);
            };
            return;
        }
        if (idx_in_parent == parent->children_size() - 1) {
            left_left = std::make_unique<Node<K_t, V_t>>(parent->get_child_id(idx_in_parent - 2), &pager);
            if (left_left->can_lend()) {
                redistributeInternal3to3(*left_left, *left, node, *parent, idx_in_parent - 2);
            } else {
                mergeInternal3to2(*left_left, *left, node, parentStack, idx_in_parent - 2);
            }
            return;
        }
        mergeInternal3to2(*left, node, *right, parentStack, idx_in_parent - 1);
    }

    void mergeInternal2to1(Node<K_t, V_t>& left, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack) {
        if (DEBUG) {
            std::cout << "BTree::mergeInternal2to1(left=" << left.id() << ", right=" << right.id() << ")" << std::endl;
        }

        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);
        
        assert(parent->id() == root_id() && parent->children_size() == 2);
        
        auto mid_key = parent->get_key(0);
        left.take_from_right_internal(right, right.children_size(), mid_key);
        set_root_id(left.id());
        parent->mark_deleted();
        right.mark_deleted();
    }

    void mergeInternal3to2(Node<K_t, V_t>& left, Node<K_t, V_t>& middle, Node<K_t, V_t>& right, std::stack<std::pair<node_size_t, int>>& parentStack, node_size_t left_idx) {
        if (DEBUG) {
            std::cout << "BTree::mergeInternal3to2(left=" << left.id() << ", middle=" << middle.id() << ", right=" << right.id() << ")" << std::endl;
        }

        auto total = left.children_size() + middle.children_size() + right.children_size();
        auto s1 = total / 2;
        auto s2 = total - s1;
        
        auto [parent_id, idx_in_parent] = parentStack.top();
        auto parent = std::make_unique<Node<K_t, V_t>>(parent_id, &pager);

        auto old_mid_key_1 = parent->get_key(left_idx);
        auto old_mid_key_2 = parent->get_key(left_idx + 1);

        auto new_mid_key_1 = left.take_from_right_internal(middle, s1 - left.children_size(), old_mid_key_1);
        middle.take_from_right_internal(right, s2 - middle.children_size(), old_mid_key_2);
        middle.set_next(right.get_next_id());

        assert(left.size() == s1);
        assert(middle.size() == s2);
        assert(right.size() == 0);

        parent->set_key(new_mid_key_1, left_idx);
        parent->remove_from_internal(left_idx + 1);
        right.mark_deleted();

        if (parent->id() != root_id() && parent->underflow()) {
            parentStack.pop();
            handleInternalUnderflow(*parent, parentStack);
        }
    }

    void printNode(std::ostream& os, node_size_t node_id, int depth) {
        if (node_id == -1) {
            return;
        }
        std::string indent(depth * 2, ' ');
        auto node = std::make_unique<Node<K_t, V_t>>(node_id, &pager);
        if (node->is_leaf()) {
            os << indent << "-" << " Leaf[" << node->id() << "](" << node->size() << "): ";
            for (node_size_t i = 0; i < node->size(); ++i) {
                os << node->get_key(i) << " ";
            }
            os << "\n";
        } else {
            os << indent << "-" << " Internal[" << node->id() << "](" << node->size() << "); ";
            for (node_size_t i = 0; i < node->size(); ++i) {
                os << node->get_key(i) << " ";
            }
            os << "\n";
            for (node_size_t i = 0; i < node->children_size(); ++i) {
                printNode(os, node->get_child_id(i), depth + 1);
            }
        }
    }

public:
    bool check_integrity() {
        if (root_id() == -1) {
            return true;
        }
        auto [childMin, childMax, childValid] = check_node_integrity(root_id(), false);
        return childValid;
    }

private:
    std::tuple<K_t, K_t, bool> check_node_integrity(node_size_t node_id, bool isRootChild) {
        auto node = std::make_unique<Node<K_t, V_t>>(node_id, &pager);
        int minAllowed;
        if (isRootChild) {
            if (node->is_leaf()) {
                minAllowed = Node<K_t, V_t>::MAX_KEYS_LEAF / 2;
            } else {
                minAllowed = Node<K_t, V_t>::MAX_KEYS_INTERNAL / 2;
            }
        } else {
            if (node->is_leaf()) {
                minAllowed = Node<K_t, V_t>::MIN_KEYS_LEAF;
            } else {
                minAllowed = Node<K_t, V_t>::MIN_KEYS_INTERNAL;
            }
        }
        bool result = (node->id() == root_id() || !(node->size() < minAllowed)) && !node->overflow();
        if (!result) {
            if (DEBUG) { std::cout << "IntegrityError: node[" << node->id() << "]. (node->id() == root_id() || !(node->size() < minAllowed)) && !node->overflow();" << std::endl; }
            if (DEBUG) { std::cout << "root_id()==" << root_id() << ", node->size()==" << node->size() << ", minAllowed==" << minAllowed << ", node->overflow()==" << node->overflow() << std::endl; }
        }
        for (node_size_t i = 0; i + 1 < node->size(); ++i) {
            if (node->get_key(i) >= node->get_key(i + 1)) {
                if (DEBUG) { std::cout << "IntegrityError: node[" << node->id() << "].get_key(" << i << ")==" << node->get_key(i) << " >= " << "node.get_key(" << i + 1 << ")==" << node->get_key(i + 1) << std::endl; }
                result = false;
            }
        }
        if (node->is_leaf()) {
            if (node->empty()) {
                if (DEBUG) { std::cout << "IntegrityError: node[" << node->id() << "].empty() == true" << std::endl; }
                return {K_t(), K_t(), false};
            }
            return {node->get_key(0), node->get_key(node->size() - 1), result};
        }
        if (node->children_size() == 0) {
            return {K_t(), K_t(), false};
        }
        K_t globalMin, globalMax;
        for (node_size_t i = 0; i < node->children_size(); ++i) {
            auto [childMin, childMax, childValid] = check_node_integrity(node->get_child_id(i), node->id() == root_id());
            result &= childValid;
            if (i > 0) {
                if (node->get_key(i - 1) > childMin) {
                    if (DEBUG) { std::cout << "IntegrityError: node[" << node->id() << "].get_key(" << i - 1 << ")==" << node->get_key(i - 1) << " > childMin==" << childMin << std::endl; }
                    result = false;
                }
            }
            if (i < node->size()) {
                if (childMax >= node->get_key(i)) {
                    if (DEBUG) { std::cout << "IntegrityError: node[" << node->id() << "].get_key(" << i << ")==" << node->get_key(i) << " <= childMax==" << childMax << std::endl; }
                    result = false;
                }
            }
            if (i == 0) {
                globalMin = childMin;
            }
            if (i == node->children_size() - 1) {
                globalMax = childMax;
            }
        }
        return {globalMin, globalMax, result};
    }
};

#endif  // QUASARDB_B_STAR_PLUS_TREE_H