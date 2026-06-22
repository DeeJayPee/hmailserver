// Copyright (c) 2006 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "stdafx.h"

#include "IMAPFolders.h"
#include "IMAPFolder.h"

#include "../Util/Time.h"
#include "../../IMAP/IMAPConfiguration.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{


   IMAPFolders::IMAPFolders(__int64 iAccountID, __int64 iParentFolderID) :
      account_id_(iAccountID),
      parent_folder_id_(iParentFolderID)
   {

   }

   IMAPFolders::IMAPFolders() :
      account_id_(0),
      parent_folder_id_(0)
   {

   }


   IMAPFolders::~IMAPFolders()
   {
  
   }

   void
   IMAPFolders::Refresh()
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      vecObjects.clear();

      SQLCommand command("select folderid, folderparentid, foldername, folderissubscribed, foldercurrentuid, foldercreationtime from hm_imapfolders "
                         " where folderaccountid = @FOLDERACCOUNTID order by folderid asc");

      command.AddParameter("@FOLDERACCOUNTID", account_id_);
   
      std::shared_ptr<DALRecordset> pRS = Application::Instance()->GetDBManager()->OpenRecordset(command);
      if (!pRS)
         return;

      std::vector<std::pair<__int64, std::shared_ptr<IMAPFolder> > > vecIMAPFolders;
      std::map<__int64, std::shared_ptr<IMAPFolder> > foldersByID;
      std::map<__int64, __int64> parentByID;

      if (!pRS->IsEOF())
      {
         __int64 iFolderID = 0;
         __int64 iParentID = 0;
         String sFolderName;
         bool bIsSubscribed = false;   
         bool bShared = false;
         unsigned int currentUID = 0;
         DateTime creationTime;

         while (!pRS->IsEOF())
         {
            iFolderID = pRS->GetLongValue("folderid");
            iParentID = pRS->GetLongValue("folderparentid");
            sFolderName = pRS->GetStringValue("foldername");
            bIsSubscribed = (pRS->GetLongValue("folderissubscribed") == 1) ? true : false;
            currentUID = (unsigned int) pRS->GetInt64Value("foldercurrentuid");
            creationTime = Time::GetDateFromSystemDate(pRS->GetStringValue("foldercreationtime"));

            // Initialize with dummy parent folder. We can't set it here since it may not
            // even be loaded from the recordset yet.
            std::shared_ptr<IMAPFolder> pFolder = std::shared_ptr<IMAPFolder>(new IMAPFolder(account_id_, iParentID));
            
            pFolder->SetID(iFolderID);
            pFolder->SetFolderName(sFolderName);
            pFolder->SetIsSubscribed(bIsSubscribed);
            pFolder->SetCurrentUID(currentUID);
            pFolder->SetCreationTime(creationTime);

            vecIMAPFolders.push_back(std::make_pair(iParentID, pFolder));
            foldersByID[iFolderID] = pFolder;
            parentByID[iFolderID] = iParentID;

            pRS->MoveNext();
         }

         __int64 inboxFolderID = -1;
         for (auto folder : vecIMAPFolders)
         {
            std::shared_ptr<IMAPFolder> pFolder = folder.second;
            if (pFolder->GetFolderName().CompareNoCase(_T("INBOX")) == 0)
            {
               if (folder.first == -1)
               {
                  inboxFolderID = pFolder->GetID();
                  break;
               }

               if (inboxFolderID == -1)
                  inboxFolderID = pFolder->GetID();
            }
         }

         std::set<__int64> repairedFolderIDs;
         for (auto folder : vecIMAPFolders)
         {
            std::shared_ptr<IMAPFolder> pFolder = folder.second;
            __int64 folderID = pFolder->GetID();
            __int64 parentID = parentByID[folderID];

            if (parentID == 0)
            {
               if (inboxFolderID > 0 && inboxFolderID != folderID)
                  parentID = inboxFolderID;
               else
                  parentID = -1;

               parentByID[folderID] = parentID;
               repairedFolderIDs.insert(folderID);
            }

            if (parentID == folderID)
            {
               parentByID[folderID] = -1;
               repairedFolderIDs.insert(folderID);
               continue;
            }

            if (parentID != -1 && foldersByID.find(parentID) == foldersByID.end())
            {
               parentByID[folderID] = -1;
               repairedFolderIDs.insert(folderID);
               continue;
            }
         }

         std::map<__int64, int> folderVisitState;
         for (auto folder : vecIMAPFolders)
         {
            std::shared_ptr<IMAPFolder> pFolder = folder.second;
            __int64 folderID = pFolder->GetID();

            std::vector<__int64> visitedFolderIDs;
            __int64 currentFolderID = folderID;
            while (currentFolderID != -1)
            {
               if (folderVisitState[currentFolderID] == 2)
                  break;

               if (folderVisitState[currentFolderID] == 1)
               {
                  parentByID[folderID] = -1;
                  repairedFolderIDs.insert(folderID);
                  break;
               }

               folderVisitState[currentFolderID] = 1;
               visitedFolderIDs.push_back(currentFolderID);

               auto iterParent = parentByID.find(currentFolderID);
               if (iterParent == parentByID.end() || iterParent->second == currentFolderID)
               {
                  parentByID[folderID] = -1;
                  repairedFolderIDs.insert(folderID);
                  break;
               }

               currentFolderID = iterParent->second;
            }

            for (__int64 visitedFolderID : visitedFolderIDs)
               folderVisitState[visitedFolderID] = 2;
         }

         for (auto folder : vecIMAPFolders)
         {
            std::shared_ptr<IMAPFolder> pFolder = folder.second;
            __int64 folderID = pFolder->GetID();
            __int64 parentID = parentByID[folderID];

            pFolder->SetParentFolderID(parentID);

            if (parentID == -1)
            {
               vecObjects.push_back(pFolder);
               continue;
            }

            auto iterParent = foldersByID.find(parentID);
            if (iterParent != foldersByID.end())
               iterParent->second->GetSubFolders()->AddItem(pFolder);
            else
               vecObjects.push_back(pFolder);
         }

         if (repairedFolderIDs.size() > 0)
         {
            String repairedFolders;
            bool addSeparator = false;
            for (__int64 folderID : repairedFolderIDs)
            {
               if (addSeparator)
                  repairedFolders += ", ";

               repairedFolders += StringParser::IntToString(folderID);
               addSeparator = true;
            }

            String sMessage;
            sMessage.Format(_T("Malformed IMAP folder parent references were repaired in memory. Account: %I64d, Folders: %s"), account_id_, repairedFolders.c_str());

            ErrorManager::Instance()->ReportError(
               ErrorManager::Medium, 5125, "IMAPFolders::Refresh()", sMessage);
         }
      }

   }

   std::shared_ptr<IMAPFolder> 
   IMAPFolders::GetFolderByName(const String &sName, bool bRecursive)
   { 
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      for(std::shared_ptr<IMAPFolder> pFolder : vecObjects)
      {
         if (pFolder->GetFolderName().Equals(sName, false))
            return pFolder;

         if (bRecursive)
         {
            // Visit this folder.
            std::shared_ptr<IMAPFolders> pSubFolders = pFolder->GetSubFolders();
            pFolder = pSubFolders->GetFolderByName(sName, bRecursive);

            if (pFolder)
               return pFolder;
         }
      }

      std::shared_ptr<IMAPFolder> pEmpty;
      return pEmpty;
   }


  


   std::shared_ptr<IMAPFolder>
   IMAPFolders::GetFolderByFullPath(const String &sPath)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      String hierarchyDelimiter = Configuration::Instance()->GetIMAPConfiguration()->GetHierarchyDelimiter();

      std::vector<String> sVecPath = StringParser::SplitString(sPath, hierarchyDelimiter);

      return GetFolderByFullPath(sVecPath);
   }

   std::shared_ptr<IMAPFolder>
   IMAPFolders::GetFolderByFullPath(const std::vector<String> &vecFolders)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      std::shared_ptr<IMAPFolder> pCurFolder;
      size_t lNoOfParts= vecFolders.size();
      for (unsigned int i = 0; i < lNoOfParts; i++)
      {
         if (pCurFolder)
         {
            String sFoldName = vecFolders[i];
            pCurFolder = pCurFolder->GetSubFolders()->GetFolderByName(sFoldName);
         }
         else
         {
            String sFoldName = vecFolders[i];
            pCurFolder = GetFolderByName(sFoldName);

            if (!pCurFolder)
               return pCurFolder;
         }
      }

      return pCurFolder;
      
   }

   void
   IMAPFolders::RemoveFolder(std::shared_ptr<IMAPFolder> pFolderToRemove)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      auto iterCurPos = vecObjects.begin();
      auto iterEnd = vecObjects.end();

      __int64 lRemoveFolderID = pFolderToRemove->GetID();
      for (; iterCurPos!= iterEnd; iterCurPos++)
      {
         std::shared_ptr<IMAPFolder> pFolder = (*iterCurPos);

         if (pFolder->GetID() == lRemoveFolderID)
         {
            // Remove this folder fro the collection.
            vecObjects.erase(iterCurPos);
            return;
         }
      }
   }
  
   void
   IMAPFolders::CreatePath(std::shared_ptr<IMAPFolders> pParentContainer,
                           const std::vector<String> &vecFolderPath, 
                           bool bAutoSubscribe)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      String hierarchyDelimiter = Configuration::Instance()->GetIMAPConfiguration()->GetHierarchyDelimiter();

      LOG_DEBUG("Creating IMAP folder " + StringParser::JoinVector(vecFolderPath, hierarchyDelimiter));

      std::vector<String> vecTempPath = vecFolderPath;

      std::shared_ptr<IMAPFolder> pParentFolder;

      while (vecTempPath.size() > 0)
      {
         // Get first level.
         String sTopLevel = vecTempPath[0];

         std::shared_ptr<IMAPFolder> pParentCheck = pParentContainer->GetFolderByName(sTopLevel, false);

         if (pParentCheck)
         {
            // This folder already exists. Create next level.
            pParentContainer = pParentCheck->GetSubFolders();
            pParentFolder = pParentCheck;
            vecTempPath = StringParser::GetAllButFirst(vecTempPath);

            continue;
         }

         __int64 iParentFolderID = -1;
         if (pParentFolder)
            iParentFolderID = pParentFolder->GetID();

         std::shared_ptr<IMAPFolder> pFolder = std::shared_ptr<IMAPFolder>(new IMAPFolder(account_id_, iParentFolderID));
         pFolder->SetFolderName(sTopLevel);
         pFolder->SetIsSubscribed(bAutoSubscribe);

         PersistentIMAPFolder::SaveObject(pFolder);

         // Add the folder to the collection.
         pParentContainer->AddItem(pFolder);

         // Go down one folder.
         pParentContainer = pFolder->GetSubFolders();
         
         vecTempPath = StringParser::GetAllButFirst(vecTempPath);
         pParentFolder = pFolder;

      }
   }

   bool
   IMAPFolders::PreSaveObject(std::shared_ptr<IMAPFolder> pObject, XNode *node)
   {
      pObject->SetAccountID(GetAccountID());
      pObject->SetParentFolderID(parent_folder_id_);
      return true;
   }


   std::shared_ptr<IMAPFolder> 
   IMAPFolders::GetItemByDBIDRecursive(__int64 lFolderID)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      auto iterCurPos = vecObjects.begin();

      for(std::shared_ptr<IMAPFolder> pFolder : vecObjects)
      {
         if (pFolder->GetID() == lFolderID)
            return pFolder;

         // Visit this folder.
         std::shared_ptr<IMAPFolders> pSubFolders = pFolder->GetSubFolders();
         pFolder = pSubFolders->GetItemByDBIDRecursive(lFolderID);

         if (pFolder)
            return pFolder;
      }


      std::shared_ptr<IMAPFolder> pEmpty;
      return pEmpty;

   }

   __int64 
   IMAPFolders::GetParentID()
   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Returns the ID of the IMAP folder in which these folder exists. If this is
   // a top level collection, -1 is returned.
   //---------------------------------------------------------------------------()
   {
      return parent_folder_id_; 
   }

   __int64 
   IMAPFolders::GetAccountID()
   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Returns the ID of the account in which these folders exist
   //---------------------------------------------------------------------------()
   {
      return account_id_; 
   }

   String 
   IMAPFolders::GetCollectionName() const 
   {
      if (GetIsPublicFolders_())
         return "PublicFolders"; 
      else
         return "Folders"; 
   }

   bool 
   IMAPFolders::GetIsPublicFolders_() const
   {
      if (account_id_ == 0)
         return true;
      else
         return false;
   }
}
