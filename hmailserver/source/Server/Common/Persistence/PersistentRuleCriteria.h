// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

namespace HM
{
   class RuleCriteria;
   enum PersistenceMode;

   class PersistentRuleCriteria
   {
   public:
      PersistentRuleCriteria(void);
      ~PersistentRuleCriteria(void);

      static bool ReadObject(shared_ptr<RuleCriteria> pRuleCriteria, const SQLCommand &sSQL);
      static bool ReadObject(shared_ptr<RuleCriteria> pRuleCriteria, shared_ptr<DALRecordset> pRS);

      
      static bool SaveObject(shared_ptr<RuleCriteria> pRuleCriteria, String &errorMessage, PersistenceMode mode);
      static bool SaveObject(shared_ptr<RuleCriteria> pRuleCriteria);
      static bool DeleteObject(shared_ptr<RuleCriteria> pRuleCriteria);

      static bool DeleteObjects(__int64 iRuleID);
   };
}