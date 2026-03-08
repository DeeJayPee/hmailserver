// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

using System;
using NUnit.Framework;
using RegressionTests.Shared;
using hMailServer;

namespace RegressionTests.Infrastructure
{
   [TestFixture]
   public class AccountProperties : TestFixtureBase
   {
      [Test]
      [Category("Accounts")]
      [Description("Ensure that non-empty account size is reported properly")]
      public void SizeAfterSend()
      {
         Domain domain = SingletonProvider<TestSetup>.Instance.AddTestDomain();
         Account account = SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test@example.test",
                                                                            "test");

         // Send a message
         for (int i = 0; i < 30; i++)
            SmtpClientSimulator.StaticSend("test@example.test", "test@example.test", "Test message",
                                           "123456789012345678901234567890123456789012345678901234567890");

         ImapClientSimulator.AssertMessageCount("test@example.test", "test", "Inbox", 30);

         float size = account.Size;
         if (size == 0)
            throw new Exception("Account is empty");
      }

      [Test]
      [Category("Accounts")]
      [Description("Ensure that empty account size is reported properly")]
      public void SizeBeforeSend()
      {
         Domain domain = SingletonProvider<TestSetup>.Instance.AddTestDomain();
         Account account = SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test@example.test", "test");

         if ((int) account.Size != 0)
            throw new Exception("Account is not empty");
      }

      [Test]
      [Category("Accounts")]
      [Description("Ensure that account size is increased when a message is received.")]
      public void SizeIncreasedWhenMessageReceived()
      {
         Domain domain = SingletonProvider<TestSetup>.Instance.AddTestDomain();
         Account account = SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test@example.test",
                                                                            "test");

         string body = TestSetup.CreateLargeDummyMailBody();

         // Send a message
         SmtpClientSimulator.StaticSend("test@example.test", "test@example.test", "Test message", body);
         ImapClientSimulator.AssertMessageCount("test@example.test", "test", "Inbox", 1);

         float sizeBefore = account.Size;

         SmtpClientSimulator.StaticSend("test@example.test", "test@example.test", "Test message", body);
         ImapClientSimulator.AssertMessageCount("test@example.test", "test", "Inbox", 2);

         float sizeAfter = account.Size;

         Assert.Greater(sizeAfter, sizeBefore);
      }
   }
}