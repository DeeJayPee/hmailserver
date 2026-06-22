// Copyright (c) 2026 hMailServer contributors

#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>

#include "IMAPResult.h"
#include "../Common/Threading/Task.h"

namespace HM
{
   class IMAPConnection;
   class IMAPCommandArgument;

   class IMAPFetchTask : public Task
   {
   public:
      IMAPFetchTask(__int64 accountID, std::function<IMAPResult()> fetchFunction, std::function<void(IMAPResult)> completionFunction);
      virtual ~IMAPFetchTask();

      __int64 GetAccountID() const { return account_id_; }

   protected:
      virtual void DoWork();

   private:
      __int64 account_id_;
      std::function<IMAPResult()> fetch_function_;
      std::function<void(IMAPResult)> completion_function_;
   };

   class IMAPFetchScheduler : public Singleton<IMAPFetchScheduler>
   {
   public:
      IMAPFetchScheduler();
      virtual ~IMAPFetchScheduler();

      void Initialize();
      void Shutdown();

      bool GetEnabled() const;
      bool IsHeavyFetch(const String &command) const;

      bool QueueFetch(__int64 accountID, std::function<IMAPResult()> fetchFunction, std::function<void(IMAPResult)> completionFunction);
      void OnTaskCompleted(__int64 accountID);

   private:
      void StartQueuedTasks_();
      bool CanStartTask_(std::shared_ptr<IMAPFetchTask> task) const;

      bool initialized_;
      size_t queue_id_;
      String queue_name_;

      std::deque<std::shared_ptr<IMAPFetchTask> > pending_tasks_;
      std::map<__int64, int> running_tasks_by_account_;
      int running_task_count_;

      boost::recursive_mutex mutex_;
   };
}
