#include "threadpool.h"
using namespace NLTP;

void Thread::createWorkingThread(unsigned int queueLen)
{
    m_alive.store(true);
    m_queueLen = queueLen;
    m_in = 0;
    m_out = 0;
    m_tasks.resize(queueLen);
    std::thread* pThread = new std::thread(&Thread::working, this);
    pThread->detach();
    m_upThread = std::unique_ptr<std::thread>(pThread);
    return;
}

void Thread::release()
{
    m_alive.store(false);
    m_upThread.reset(nullptr);
    return;
}

void Thread::addTask(std::shared_ptr<Task> spTask)
{
    m_tasks[m_in % m_queueLen] = std::move(spTask);
    m_in++;
    return;
}

std::shared_ptr<Task> Thread::pop_back()
{
    std::shared_ptr<Task> spTask = std::move(m_tasks[m_in % m_queueLen]);
    m_in--;
    return spTask;
}

int Thread::getTaskNum()
{
    int taskNum = 0;
    if (m_in > m_out) {
        taskNum = m_in - m_out;
    } else {
        taskNum = m_queueLen - m_in + m_out;
    }
    return taskNum;
}

void Thread::working()
{
    while (m_alive.load()) {
        /* get task */
        std::shared_ptr<Task> spTask = nullptr; 
        if (m_in != m_out) {
            spTask = std::move(m_tasks[m_out % m_queueLen]);
            m_out++;
        }
        /* execute task */
        if (spTask != nullptr) {
            spTask->execute();
        }
    }
    return;
}

int NonLockThreadPool::createThreads(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int queueLen)
{
    if (minThreadNum > maxThreadNum) {
        return -1;
    }
    /* init assign */
    m_minThreadNum = minThreadNum;
    m_maxThreadNum = maxThreadNum;
    m_queueLen = queueLen;
    m_aliveThreadNum = 0;
    m_policy = THREADPOOL_ROUNDROBIN;
    m_threadIndex = 0;
    m_alive.store(true);
    m_balance.store(false);
    /* create working thread */
    for (unsigned int i = 0; i < m_minThreadNum; i++) {
        std::shared_ptr<Thread> spThread = std::make_shared<Thread>();
        spThread->createWorkingThread(queueLen);
        m_threads.push_back(spThread);
        m_aliveThreadNum++;
    }
    /* create adnin thread */
    m_adminThread = std::thread(&NonLockThreadPool::admin, this);
    m_adminThread.detach();
    return 0;
}

void NonLockThreadPool::dispatchTaskByRoundRobin(std::shared_ptr<Task> spTask)
{
    int i = m_threadIndex % m_threads.size();
    m_threads.at(i)->addTask(spTask);
    m_threadIndex++;
    return;
}

void NonLockThreadPool::dispatchTaskByRand(std::shared_ptr<Task> spTask)
{
    int i = rand() % m_threads.size();
    m_threads.at(i)->addTask(spTask);
    return;
}

void NonLockThreadPool::dispatchTaskToLeastLoad(std::shared_ptr<Task> spTask)
{
    unsigned int index = 0;
    unsigned int minTaskNum = 1000000;
    /* find least load thread */
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            unsigned int taskNum = m_threads.at(i)->getTaskNum();
            if (minTaskNum > taskNum) {
                minTaskNum = taskNum;
                index = i;
            }
        }
    }
    m_threads.at(index)->addTask(spTask);
    return;
}

void NonLockThreadPool::loadBalance()
{
    /* find max-task-num and min-task-num */
    unsigned int minIndex = 0;
    unsigned int maxIndex = 0;
    unsigned int minTaskNum = 1000000;
    unsigned int maxTaskNum = 0;
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            unsigned int taskNum = m_threads.at(i)->getTaskNum();
            if (minTaskNum > taskNum) {
                minTaskNum = taskNum;
                minIndex = i;
            }
            if (maxTaskNum < taskNum) {
                maxTaskNum = taskNum;
                maxIndex = i;
            }
        }
    }
    /* balance */
    for (unsigned int i = 0; i < maxTaskNum - minTaskNum; i++) {
        std::shared_ptr<Task> spTask = m_threads.at(maxIndex)->pop_back();
        m_threads.at(minIndex)->addTask(spTask);
    }
    return;
}

int NonLockThreadPool::addTask(std::shared_ptr<Task> spTask)
{
    if (spTask == nullptr) {
        return -1;
    }
    /* load balance */
    if (m_balance.load()) {
        loadBalance();
        m_balance.store(false);
    }
    /* dispatch task */
    switch (m_policy) {
        case THREADPOOL_ROUNDROBIN:
            dispatchTaskByRoundRobin(spTask);
            break;
        case THREADPOOL_RAND:
            dispatchTaskByRand(spTask);
            break;
        case THREADPOOL_LEASTLOAD:
            dispatchTaskToLeastLoad(spTask);
            break;
        default:
            dispatchTaskByRoundRobin(spTask);
    }
    return 0;
}

int NonLockThreadPool::getThreadNum()
{ 
    return m_aliveThreadNum;
}

void NonLockThreadPool::migrateTask(unsigned int from, unsigned int to)
{
    


    return;
}
void NonLockThreadPool::increaseThread()
{
    /* do statistic */

    /* add thread */
    if (m_aliveThreadNum == m_threads.size()) {
        std::shared_ptr<Thread> spThread = std::make_shared<Thread>();
        spThread->createWorkingThread(m_queueLen);
        m_threads.push_back(spThread);
    } else {
        for (unsigned int i = 0; i < m_threads.size(); i++) {
            if (m_threads.at(i)->m_alive.load() == false) {
                m_threads.at(i)->createWorkingThread(m_queueLen);
                break;
            }
        }
    }
    m_aliveThreadNum++;
    return;
}
            
void NonLockThreadPool::decreaseThread()
{   
    /* do statistic */

    /* find least-task-num-thread */

    /* migrate task */

    /* release thread */

    m_aliveThreadNum--;
    return;
}

void NonLockThreadPool::admin()
{
    while (m_alive.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        /* load balance */
        m_balance.store(true);
        /* increase thread */
        if (false) {
            increaseThread();
        }
        /* decrease thread */
        if (false) {
            decreaseThread();
        }
    }
    return;
}
void NonLockThreadPool::shutdown()
{
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            m_threads.at(i)->m_alive.store(false);
        }
    }
    m_alive.store(false);
    return;
}
