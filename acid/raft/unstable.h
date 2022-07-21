//
// Created by zavier on 2022/7/8.
//

#ifndef ACID_UNSTABLE_H
#define ACID_UNSTABLE_H

#include "entry.h"
#include "snapshot.h"

namespace acid::raft {

/**
 * @brief 未持久化的日志
 * @details unstable结构体中的offset字段记录了unstable的日志起点，该起点可能比Storage中index
 * 最高的日志条目旧，也就是说Storage和unstable中的日志可能有部分重叠，因此在处理二者之间的日志时，
 * 有一些裁剪日志的操作。
 */
struct Unstable {
    // 日志快照，使用者某一时刻的全量序列化后的二进制数
    Snapshot::ptr snapshot;
    // 日志数组
    std::vector<Entry> entries;
    // offset是第一个Unstable日志的索引，相信有有人会疑问，直接用entries[0].Index不就可以了么？
    // 需要注意的是，在系统在很多时刻entries是空的，比如刚启动，日志持久化完成等，所以需要单独一个
    // 变量。当entries为空的时候，该值就是未来可能成为不可靠日志的第一个索引。这个变量在索引
    // entries的时候非常有帮助，比如entries[index - offset]就获取了索引为index的Entry
    int64_t offset = 0;

    /**
     * @brief 获取第一个日志的index
     * @details 后续会有很多次出现maybeXxx()系列函数，这些函数都是尝试性的操作，也就是可能会成功，
     * 也可能会失败，在返回值中会告诉操作的成功或者失败。这个函数用来获取第一个日志的索引，这里特别
     * 需要注意，因为函数有点迷惑人，这个函数需要返回的是最近快照到现在的第一个日志索引。所以说就是
     * 快照的索引，切不可把它当做是第一个不可靠日志的索引。如果查询的索引不在unstable中时，其会继续
     * 询问Storage
     * @return int64_t 成功则返回第一个日志的index，失败返回-1
     */
    [[nodiscard]]
    int64_t maybeFirstIndex() const;

    /**
     * @brief 获取最后一个日志的index，和上一个函数类似
     * @return int64_t 成功则返回最后一个日志的index，失败返回-1
     */
    [[nodiscard]]
    int64_t maybeLastIndex() const;

    /**
     * @brief 返回给定日志index的term
     * @return int64_t 失败返回-1
     */
    [[nodiscard]]
    int64_t maybeTerm(int64_t index) const;

    /**
     * @brief 使用者持久化不可靠日志后触发的调用，可靠的日志索引已经到达了index可以裁剪掉unstable中的这段日志了
     * @param index 可靠日志的 index
     * @param term 可靠日志的 term
     */
    void stableTo(int64_t index, int64_t term);

    /**
     * @brief 快照持久完成后触发的调用，删除unstable快照
     * @param index 快照的最后 index
     */
    void stableSnapTo(int64_t index);

    /**
     * @brief 接收到leader发来的快照后调用的，暂时存入unstable等待使用者持久化
     * @param snap leader发来的快照
     */
    void restore(Snapshot::ptr snap);

    /**
     * @brief 存储日志到unstable里
     * @details 这个函数是leader发来追加日志消息的时候触发调用的，raft先把这些日志存储在unstable
     * 中等待使用者持久化。为什么是追加？因为日志是有序的，leader发来的日志一般是该节点紧随其后的日志
     * 亦或是有些重叠的日志，看似像是一直追加一样
     * @param ents leader发来的日志数组
     */
    void truncateAndAppend(const std::vector<Entry>& ents);

    /**
     * @brief 截取(low，high]的日志
     */
    std::vector<Entry> slice(int64_t low, int64_t high);


    /**
    * @brief 检查索引是否正确，
     * @note offset <= low <= high <= offset+entries.size()
    */
    void mustCheckOutOfBounds(int64_t low, int64_t high) const;

};
}
#endif //ACID_UNSTABLE_H
