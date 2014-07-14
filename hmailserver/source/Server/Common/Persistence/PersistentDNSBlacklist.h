// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

namespace HM
{
   class DNSBlackList;
   enum PersistenceMode;

   class PersistentDNSBlackList
   {
   public:
      PersistentDNSBlackList(void);
      ~PersistentDNSBlackList(void);
      
      static bool DeleteObject(shared_ptr<DNSBlackList> pObject);
      static bool SaveObject(shared_ptr<DNSBlackList> pObject, String &errorMessage, PersistenceMode mode);
      static bool SaveObject(shared_ptr<DNSBlackList> pObject);
      static bool ReadObject(shared_ptr<DNSBlackList> pObject, shared_ptr<DALRecordset> pRS);

   };
}