#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

// 定义队列策略：阻塞（不丢数据）/ 丢弃旧数据（非阻塞）
enum class QueuePolicy {
    BLOCK_WHEN_FULL,   // 队列满时阻塞生产者，确保数据不丢失
    DROP_OLD_WHEN_FULL // 队列满时丢弃旧数据，只保留最新的
};

template <typename T>
class SafeQueue {
public:
    // 构造函数：指定策略和最大容量
    SafeQueue(QueuePolicy policy, size_t max_size=5)
        : policy_(policy), max_size_(max_size) {
        if (max_size == 0) {
            throw std::invalid_argument("max_size 不能为0");
        }
    }

    // 推送数据（根据策略执行不同逻辑）
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (policy_ == QueuePolicy::BLOCK_WHEN_FULL) {
            // 策略1：阻塞模式——队列满时等待，直到有空间
            not_full_.wait(lock, [this]() {
                return queue_.size() < max_size_;
            });
        } else {
            // 策略2：丢弃模式——队列满时弹出旧数据，确保新数据能入队
            while (queue_.size() >= max_size_) {
                std::cerr << "\n" << "丢失的：" << queue_.front() <<std::endl;
                queue_.pop(); // 丢弃最旧的数据
                
            }
        }

        queue_.push(item);
        not_empty_.notify_one(); // 通知消费者有新数据
    }

    // 弹出数据（两种策略共用，超时返回false）
    bool pop(T& item, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待队列非空（超时返回false）
        bool has_data = not_empty_.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [this]() { return !queue_.empty(); }
        );

        if (!has_data) {
            return false; // 超时，未取到数据
        }

        // 取出数据
        item = queue_.front();
        queue_.pop();

        // 若为阻塞模式，通知生产者队列有空间了
        if (policy_ == QueuePolicy::BLOCK_WHEN_FULL) {
            not_full_.notify_one();
        }

        return true;
    }

    // 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        // 清空后通知阻塞的生产者（仅阻塞模式）
        if (policy_ == QueuePolicy::BLOCK_WHEN_FULL) {
            not_full_.notify_all();
        }
    }

    // 获取当前队列大小（线程安全）
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;               // 底层队列
    mutable std::mutex mutex_;          // 互斥锁（mutable允许const方法使用）
    std::condition_variable not_empty_; // 通知消费者“队列非空”
    std::condition_variable not_full_;  // 通知生产者“队列非满”（仅阻塞模式用）
    QueuePolicy policy_;                // 当前策略
    size_t max_size_;                   // 最大容量
};

#endif /* SAFE_QUEUE_H */
