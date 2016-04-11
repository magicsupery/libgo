#include "io_wait.h"
#include <sys/epoll.h>
#include "scheduler.h"

namespace co
{

IoWait::IoWait()
{
    loop_index_ = 0;
    epollwait_ms_ = 0;
    epoll_event_size_ = 1024;
    epoll_owner_pid_ = 0;
    epoll_fd_ = -1;
}

void IoWait::DelayEventWaitTime()
{
    ++epollwait_ms_;
    epollwait_ms_ = std::min<int>(epollwait_ms_, g_Scheduler.GetOptions().max_sleep_ms);
}

void IoWait::ResetEventWaitTime()
{
    epollwait_ms_ = 0;
}

static uint32_t PollEvent2Epoll(short events)
{
    uint32_t e = 0;
    if (events & POLLIN)   e |= EPOLLIN;
    if (events & POLLOUT)  e |= EPOLLOUT;
    if (events & POLLHUP)  e |= EPOLLHUP;
    if (events & POLLERR)  e |= EPOLLERR;
    return e;
}

static short EpollEvent2Poll(uint32_t events)
{
    short e = 0;
    if (events & EPOLLIN)  e |= POLLIN;
    if (events & EPOLLOUT) e |= POLLOUT;
    if (events & EPOLLHUP) e |= POLLHUP;
    if (events & EPOLLERR) e |= POLLERR;
    return e;
}

static std::string EpollEvent2Str(uint32_t events)
{
    std::string e("|");
    if (events & EPOLLIN)  e += "POLLIN|";
    if (events & EPOLLOUT) e += "POLLOUT|";
    if (events & EPOLLHUP) e += "POLLHUP|";
    if (events & EPOLLERR) e += "POLLERR|";
    return e;
}

void IoWait::CoSwitch()
{
    Task* tk = g_Scheduler.GetCurrentTask();
    if (!tk) return ;

    tk->state_ = TaskState::io_block;
    DebugPrint(dbg_ioblock, "task(%s) CoSwitch", tk->DebugInfo());
    g_Scheduler.CoYield();
}

void IoWait::SchedulerSwitch(Task* tk)
{
    wait_io_sentries_.push(tk->io_sentry_.get()); // A
    if (tk->io_sentry_->io_state_ == IoSentry::triggered) // B
        __IOBlockTriggered(tk->io_sentry_);
}

void IoWait::IOBlockTriggered(IoSentryPtr io_sentry)
{
    if (io_sentry->switch_state_to_triggered()) // B
        __IOBlockTriggered(io_sentry);
}

void IoWait::__IOBlockTriggered(IoSentryPtr io_sentry)
{
    assert(io_sentry->io_state_ == IoSentry::triggered);
    if (wait_io_sentries_.erase(io_sentry.get())) // A
        g_Scheduler.AddTaskRunnable(io_sentry->task_ptr_.get());
}

int IoWait::reactor_ctl(int epoll_ctl_mod, int fd, uint32_t poll_events, bool is_socket)
{
    if (is_socket) {
        epoll_event ev;
        ev.events = PollEvent2Epoll(poll_events);
        ev.data.fd = fd;
        return epoll_ctl(GetEpollFd(), epoll_ctl_mod, fd, &ev);
    }

    // TODO: poll模拟
    errno = EPERM;
    return -1;
}
int IoWait::WaitLoop(bool enable_block)
{
    if (!IsEpollCreated())
        return -1;

    // TODO: epoll多线程触发, poll单线程触发.
    std::unique_lock<LFLock> lock(epoll_lock_, std::defer_lock);
    if (!lock.try_lock())
        return -1;

    ++loop_index_;
    static epoll_event *evs = new epoll_event[epoll_event_size_];

    int timeout = enable_block ? epollwait_ms_ : 0;
retry:
    int n = epoll_wait(GetEpollFd(), evs, epoll_event_size_, timeout);
    if (n == -1) {
        if (errno == EINTR) {
            goto retry;
        }

        return 0;
    }

    DebugPrint(dbg_scheduler, "epollwait returns: %d", n);

    TriggerSet triggers;
    for (int i = 0; i < n; ++i)
    {
        int fd = evs[i].data.fd;
        FdCtxPtr fd_ctx = FdManager::getInstance().get_fd_ctx(fd);
        DebugPrint(dbg_ioblock, "epoll trigger fd(%d) events(%s) has_ctx(%d)",
                fd, EpollEvent2Str(evs[i].events).c_str(), !!fd_ctx);
        assert(fd_ctx); // 必须有context, 否则就是close流程出问题了.

        // 暂存, 最后再执行Trigger, 以便于poll可以得到更多的事件触发.
        fd_ctx->reactor_trigger(EpollEvent2Poll(evs[i].events), triggers);
    }

    // TODO: run poll

    // 触发事件, 唤醒等待中的协程.
    // 过时的唤醒由于已不在wait列表中, 
    // 会被IOBlockTriggered中的原子操作switch_state_to_triggered根据返回值过滤掉.
    for (IoSentryPtr & sentry : triggers)
        IOBlockTriggered(sentry);

    return n;
}

int IoWait::GetEpollFd()
{
    CreateEpoll();
    return epoll_fd_;
}

void IoWait::CreateEpoll()
{
    pid_t pid = getpid();
    if (epoll_owner_pid_ == pid) return ;
    std::unique_lock<LFLock> lock(epoll_create_lock_);
    if (epoll_owner_pid_ == pid) return ;

    epoll_owner_pid_ = pid;
    epoll_event_size_ = g_Scheduler.GetOptions().epoll_event_size;

    if (epoll_fd_ >= 0)
        close(epoll_fd_);

    epoll_fd_ = epoll_create(epoll_event_size_);
    if (epoll_fd_ != -1)
        DebugPrint(dbg_ioblock, "create epoll success. epollfd=%d", epoll_fd_);
    else {
        fprintf(stderr,
                "CoroutineScheduler init failed. epoll create error:%s\n",
                strerror(errno));
        exit(1);
    }
}

bool IoWait::IsEpollCreated() const
{
    return epoll_owner_pid_ == getpid();
}

} //namespace co
