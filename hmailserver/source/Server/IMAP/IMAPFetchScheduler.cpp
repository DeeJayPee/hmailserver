// Copyright (c) 2026 hMailServer contributors

#include "stdafx.h"

#include "IMAPFetchScheduler.h"
#include "IMAPFetchParser.h"

#include "../Common/Application/IniFileSettings.h"
#include "../Common/Threading/WorkQueueManager.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPFetchTask::IMAPFetchTask(__int64 accountID, std::function<IMAPResult()> fetchFunction, std::function<void(IMAPResult)> completionFunction) :
      Task("IMAPFetchTask"),
      account_id_(accountID),
      fetch_function_(fetchFunction),
      completion_function_(completionFunction)
   {

   }

   IMAPFetchTask::~IMAPFetchTask()
   {

   }

   void
   IMAPFetchTask::DoWork()
   {
      IMAPResult result(IMAPResult::ResultNo, "IMAP FETCH failed");

      try
      {
         result = fetch_function_();
      }
      catch (...)
      {
         result = IMAPResult(IMAPResult::ResultNo, "IMAP FETCH failed");
      }

      try
      {
         completion_function_(result);
      }
      catch (...)
      {
      }

      IMAPFetchScheduler::Instance()->OnTaskCompleted(account_id_);
   }

   IMAPFetchScheduler::IMAPFetchScheduler() :
      initialized_(false),
      queue_id_(0),
      queue_name_("IMAP fetch queue"),
      running_task_count_(0)
   {

   }

   IMAPFetchScheduler::~IMAPFetchScheduler()
   {

   }

   void
   IMAPFetchScheduler::Initialize()
   {
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);

      if (initialized_)
         return;

      if (!GetEnabled())
      {
         LOG_APPLICATION("IMAP fetch isolation is disabled. Legacy inline FETCH handling is active.")
         return;
      }

      int maxTasks = IniFileSettings::Instance()->GetMaxNumberOfIMAPFetchTasks();
      queue_id_ = WorkQueueManager::Instance()->CreateWorkQueue(maxTasks, queue_name_);
      initialized_ = true;

      String message;
      message.Format(_T("IMAP fetch isolation is enabled. MaxNumberOfIMAPFetchTasks=%d, MaxNumberOfIMAPFetchTasksPerAccount=%d, IMAPFetchWriteBufferLimitKB=%d"),
         maxTasks,
         IniFileSettings::Instance()->GetMaxNumberOfIMAPFetchTasksPerAccount(),
         IniFileSettings::Instance()->GetIMAPFetchWriteBufferLimitKB());
      LOG_APPLICATION(message)
   }

   void
   IMAPFetchScheduler::Shutdown()
   {
      bool removeQueue = false;

      {
         boost::lock_guard<boost::recursive_mutex> guard(mutex_);

         pending_tasks_.clear();
         running_tasks_by_account_.clear();
         running_task_count_ = 0;

         if (!initialized_)
            return;

         initialized_ = false;
         queue_id_ = 0;
         removeQueue = true;
      }

      if (removeQueue)
         WorkQueueManager::Instance()->RemoveQueue(queue_name_);
   }

   bool
   IMAPFetchScheduler::GetEnabled() const
   {
      return IniFileSettings::Instance()->GetEnableIMAPFetchIsolation();
   }

   bool
   IMAPFetchScheduler::IsHeavyFetch(const String &command) const
   {
      IMAPFetchParser parser;
      IMAPResult parseResult = parser.ParseCommand(command);
      if (parseResult.GetResult() != IMAPResult::ResultOK)
         return false;

      return parser.GetShowEnvelope() ||
             parser.GetShowBodyStructure() ||
             parser.GetShowBodyStructureNonExtensible() ||
             parser.GetPartsToLookAt().size() > 0;
   }

   bool
   IMAPFetchScheduler::QueueFetch(__int64 accountID, std::function<IMAPResult()> fetchFunction, std::function<void(IMAPResult)> completionFunction)
   {
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);

      if (!initialized_)
         return false;

      std::shared_ptr<IMAPFetchTask> task = std::shared_ptr<IMAPFetchTask>(new IMAPFetchTask(accountID, fetchFunction, completionFunction));
      pending_tasks_.push_back(task);
      StartQueuedTasks_();

      return true;
   }

   void
   IMAPFetchScheduler::OnTaskCompleted(__int64 accountID)
   {
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);

      if (running_task_count_ > 0)
         running_task_count_--;

      auto iterAccount = running_tasks_by_account_.find(accountID);
      if (iterAccount != running_tasks_by_account_.end())
      {
         iterAccount->second--;
         if (iterAccount->second <= 0)
            running_tasks_by_account_.erase(iterAccount);
      }

      StartQueuedTasks_();
   }

   void
   IMAPFetchScheduler::StartQueuedTasks_()
   {
      if (!initialized_)
         return;

      auto iterTask = pending_tasks_.begin();
      while (iterTask != pending_tasks_.end())
      {
         std::shared_ptr<IMAPFetchTask> task = (*iterTask);
         if (!CanStartTask_(task))
         {
            iterTask++;
            continue;
         }

         running_task_count_++;
         running_tasks_by_account_[task->GetAccountID()]++;

         WorkQueueManager::Instance()->AddTask(queue_id_, task);
         iterTask = pending_tasks_.erase(iterTask);
      }
   }

   bool
   IMAPFetchScheduler::CanStartTask_(std::shared_ptr<IMAPFetchTask> task) const
   {
      int maxTasks = IniFileSettings::Instance()->GetMaxNumberOfIMAPFetchTasks();
      if (running_task_count_ >= maxTasks)
         return false;

      int accountTaskCount = 0;
      auto iterAccount = running_tasks_by_account_.find(task->GetAccountID());
      if (iterAccount != running_tasks_by_account_.end())
         accountTaskCount = iterAccount->second;

      return accountTaskCount < IniFileSettings::Instance()->GetMaxNumberOfIMAPFetchTasksPerAccount();
   }
}
