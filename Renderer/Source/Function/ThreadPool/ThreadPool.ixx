module;
export module ThreadPool;
import std;
export class ThreadPool
{
public:
    ThreadPool(int t_numThreads) : m_workingNum(0), m_isStop(false)
    {

        for (int i = 0; i < t_numThreads; i++)
        {
            m_threads.emplace_back([this] { // 为每一个线程提供一个lamda函数作为他们的工作
                bool wait = false;
                while (1)
                {
                    std::unique_lock<std::mutex> lock(m_mtx); // 访问任务队列时上锁
                    // 等待, 直到队列非空或者要求停止
                    m_condition.wait(lock, [this]
                                     { return !m_tasks.empty() || m_isStop; });
                    // 如果要求停止且队列为空,中止当前线程，否则优先完成剩余工作
                    if (m_isStop && m_tasks.empty())
                    {
                        return;
                    }

                    std::function<void()> task(std::move(m_tasks.front()));
                    m_tasks.pop();
                    lock.unlock();
                    m_mtxWorking.lock(); // 当前工作进程数+1
                    m_workingNum += 1;
                    m_mtxWorking.unlock();

                    task();

                    m_mtxWorking.lock(); // 当前工作进程数-1
                    lock.lock();
                    m_workingNum -= 1;

                    if (m_tasks.empty() && m_workingNum == 0) // 当任务队列为空，且当前无运行线程时，通知wait函数
                    {
                        m_conditionWait.notify_one();
                    }
                    lock.unlock();
                    m_mtxWorking.unlock();
                }
            });
        }
    }
    ThreadPool() : m_isStop(false)
    {
        for (int i = 0; i < std::thread::hardware_concurrency(); i++)
        {
            m_threads.emplace_back([this] { // 为每一个线程提供一个lamda函数作为他们的工作
                bool wait = false;
                while (1)
                {
                    std::unique_lock<std::mutex> lock(m_mtx); // 访问任务队列时上锁
                    // 等待, 直到队列非空或者要求停止
                    m_condition.wait(lock, [this]
                                     { return !m_tasks.empty() || m_isStop; });
                    // 如果要求停止且队列为空,中止当前线程，否则优先完成剩余工作
                    if (m_isStop && m_tasks.empty())
                    {
                        return;
                    }
                    std::function<void()> task(std::move(m_tasks.front()));
                    m_tasks.pop();
                    if (m_tasks.empty())
                        wait = true;
                    lock.unlock();
                    task();
                    if (wait == true)
                    {
                        m_conditionWait.notify_one();
                        wait = false;
                    }
                }
            });
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_isStop = true;
        }
        m_condition.notify_all(); // 通知所有线程wait函数检查是否结束等待
        for (auto &t : m_threads)
        {
            t.join();
        }
    }
    template <class F, class... Args>
    void Enqueue(F &&f, Args &&...args) // 引用与转发
    {
        std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...); // 转发
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_tasks.emplace(std::move(task));
        }
        m_condition.notify_one();
    }
    /// @brief 等待之前所有提交的任务结束
    void Wait()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_conditionWait.wait(lock, [this]
                             { return m_tasks.empty(); });
    }

private:
    std::mutex m_mtxWorking;
    int m_workingNum;
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_mtx;
    std::condition_variable m_condition;

    std::mutex m_mtxWait;
    std::condition_variable m_conditionWait;

    bool m_isStop;
};