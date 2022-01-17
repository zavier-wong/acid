//
// Created by zavier on 2022/1/17.
//

#ifndef ACID_LRU_MAP_H
#define ACID_LRU_MAP_H
#include <list>
#include <optional>
#include <unordered_map>
namespace acid {

template<typename Key, typename Val>
class LRUMap {
public:
    LRUMap(size_t capacity): m_capacity(capacity) { }

    void set(Key key, Val value) {
        auto it = m_map.find(key);
        // 不存在key，插入数据
        if (it == m_map.end()) {
            // 插入之前先判断数据是否已满，如果已满，淘汰最老的数据
            if (m_map.size() >= m_capacity) {
                m_map.erase(m_list.front());
                m_list.pop_front();
            }
            auto [it2, b] = m_map.emplace(key, MapValue{value});
            it = it2;
        } else {
            m_list.erase(it->second.node_list_iter);

        }
        it->second.node_list_iter = m_list.insert(m_list.end(), key);
        it->second.value = value;
    }
    std::optional<Val> get(Key key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return {};
        }
        // 找到数据，更新key为最新
        m_list.erase(it->second.node_list_iter);
        it->second.node_list_iter = m_list.insert(m_list.end(), key);
        return it->second.value;
    }
    void remove(Key key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return;
        }
        m_list.erase(it->second.node_list_iter);
        m_map.erase(it);
    }
    size_t size() {
        return m_map.size();
    }
private:
    struct MapValue {
        Val value;
        typename std::list<Key>::iterator node_list_iter;
    };
private:
    // LRU缓存的数据条数
    const size_t m_capacity;
    // LRU存储的数据
    std::unordered_map<Key, MapValue> m_map;
    // 存储key的链表，头节点为最老的节点，尾节点为最新的的节点
    std::list<Key> m_list;
};

}
#endif //ACID_LRU_MAP_H
