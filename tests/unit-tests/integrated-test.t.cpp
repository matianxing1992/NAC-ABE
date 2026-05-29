/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017-2023, Regents of the University of California.
 *
 * This file is part of NAC-ABE.
 *
 * NAC-ABE is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * NAC-ABE is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * NAC-ABE, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of NAC-ABE authors and contributors.
 */

#include "attribute-authority.hpp"
#include "consumer.hpp"
#include "data-owner.hpp"
#include "producer.hpp"
#include "cache-producer.hpp"
#include "algo/abe-support.hpp"

#include "test-common.hpp"

#include <chrono>
#include <future>


#include <ndn-cxx/util/dummy-client-face.hpp>

namespace ndn {
namespace nacabe {
namespace tests {

const uint8_t PLAIN_TEXT[1024] = {1};

NDN_LOG_INIT(Test.IntegratedTest);

class TestIntegratedFixture : public IdentityManagementTimeFixture
{
public:
  TestIntegratedFixture()
    // Keep packet logging enabled, matching the other DummyClientFace-based
    // tests. Current ndn-cxx DummyClientFace uses this test-side bookkeeping
    // in the manual receive/validation flows exercised below.
    : producerFace(io, m_keyChain, {true, true})
    , aaFace(io, m_keyChain, {true, true})
    , tokenIssuerFace(io, m_keyChain, {true, true})
    , consumerFace1(io, m_keyChain, {true, true})
    , consumerFace2(io, m_keyChain, {true, true})
    , dataOwnerFace(io, m_keyChain, {true, true})
  {
    producerFace.linkTo(aaFace);
    producerFace.linkTo(tokenIssuerFace);
    producerFace.linkTo(consumerFace1);
    producerFace.linkTo(consumerFace2);
    producerFace.linkTo(dataOwnerFace);

    security::pib::Identity anchorId = addIdentity("/example");
    anchorCert = anchorId.getDefaultKey().getDefaultCertificate();
    saveCertToFile(anchorCert, "example-trust-anchor.cert");
    security::pib::Identity consumerId1 = addIdentity("/example/consumer1", RsaKeyParams());
    addSubCertificate("/example/consumer1", anchorId);
    consumerCert1 = consumerId1.getDefaultKey().getDefaultCertificate();

    security::pib::Identity consumerId2 = addIdentity("/example/consumer2", RsaKeyParams());
    addSubCertificate("/example/consumer1", anchorId);
    consumerCert2 = consumerId2.getDefaultKey().getDefaultCertificate();

    security::pib::Identity producerId = addIdentity("/example/producer");
    addSubCertificate("/example/producer", anchorId);
    producerCert = producerId.getDefaultKey().getDefaultCertificate();

    security::pib::Identity dataOwnerId = addIdentity("/example/dataOwner");
    addSubCertificate("/example/dataOwner", anchorId);
    dataOwnerCert = dataOwnerId.getDefaultKey().getDefaultCertificate();

    security::pib::Identity tokenIssuerId = addIdentity("/example/tokenIssuer");
    addSubCertificate("/example/tokenIssuer", anchorId);
    tokenIssuerCert = tokenIssuerId.getDefaultKey().getDefaultCertificate();

    security::pib::Identity authorityId = addIdentity("/example/authority");
    addSubCertificate("/example/authority", anchorId);
    aaCert = authorityId.getDefaultKey().getDefaultCertificate();

    signingInfo = signingByCertificate(producerCert);
  }

protected:
  DummyClientFace producerFace;
  DummyClientFace aaFace;
  DummyClientFace tokenIssuerFace;
  DummyClientFace consumerFace1;
  DummyClientFace consumerFace2;
  DummyClientFace dataOwnerFace;

  security::Certificate aaCert;
  security::Certificate anchorCert;
  security::Certificate tokenIssuerCert;
  security::Certificate consumerCert1;
  security::Certificate consumerCert2;
  security::Certificate producerCert;
  security::Certificate dataOwnerCert;
  security::SigningInfo signingInfo;
};

BOOST_FIXTURE_TEST_SUITE(TestIntegrated, TestIntegratedFixture)

BOOST_AUTO_TEST_CASE(Cp)
{
  // set up AA
  NDN_LOG_INFO("Create Attribute Authority. AA prefix: " << aaCert.getIdentity());
  security::ValidatorConfig validator(aaFace);
  validator.load("trust-schema.conf");
  CpAttributeAuthority aa(aaCert, aaFace, validator, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  // define attr list for consumer rights
  std::list<std::string> attrList = {"attr1", "attr3"};
  NDN_LOG_INFO("Add comsumer 1 "<<consumerCert1.getIdentity()<<" with attributes: attr1, attr3");
  aa.addNewPolicy(consumerCert1, attrList);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 1);

  std::list<std::string> attrList1 = {"attr1"};
  NDN_LOG_INFO("Add comsumer 2 "<<consumerCert2.getIdentity()<<" with attributes: attr1");
  aa.addNewPolicy(consumerCert2, attrList1);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 2);

  // set up consumer
  NDN_LOG_INFO("Create Consumer 1. Consumer 1 prefix:"<<consumerCert1.getIdentity());
  security::ValidatorConfig validator1(consumerFace1);
  validator1.load("trust-schema.conf");
  Consumer consumer1(consumerFace1, m_keyChain, validator1, consumerCert1, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer1.m_paramFetcher.getPublicParams().m_pub != "");

  // set up consumer
  NDN_LOG_INFO("Create Consumer 2. Consumer 2 prefix:"<<consumerCert2.getIdentity());
  security::ValidatorConfig validator2(consumerFace2);
  validator2.load("trust-schema.conf");
  Consumer consumer2(consumerFace2, m_keyChain, validator2, consumerCert2, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer2.m_paramFetcher.getPublicParams().m_pub != "");

  // set up producer
  NDN_LOG_INFO("Create Producer. Producer prefix:"<<producerCert.getIdentity());
  security::ValidatorConfig validator3(producerFace);
  validator3.load("trust-schema.conf");
  Producer producer(producerFace, m_keyChain, validator3, producerCert, aaCert, dataOwnerCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(producer.m_paramFetcher.getPublicParams().m_pub != "");

  // set up data owner
  NDN_LOG_INFO("Create Data Owner. Data Owner prefix:" << dataOwnerCert.getIdentity());
  DataOwner dataOwner(dataOwnerCert, dataOwnerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  NDN_LOG_INFO("\n=================== start work flow ==================\n");

  Name dataName = "/dataName";
  std::string policy = "(attr1 or attr2) and attr3";

  bool isPolicySet = false;
  dataOwner.commandProducerPolicy(producerCert.getIdentity(), dataName, policy,
    [&] (const Data& response) {
      NDN_LOG_DEBUG("on policy set data callback");
      isPolicySet = true;
      BOOST_CHECK_EQUAL(readString(response.getContent()), "success");
      auto policyFound = producer.findMatchedPolicy(dataName);
      BOOST_CHECK(policyFound == policy);
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );

  NDN_LOG_DEBUG("Before policy set");
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isPolicySet);

  SPtrVector<ndn::Data> contentData, ckData;
  auto policyFound = producer.findMatchedPolicy(dataName);

  std::tie(contentData, ckData) = producer.produce(dataName, policyFound, PLAIN_TEXT, signingInfo);
  BOOST_CHECK(contentData.size() > 0);
  BOOST_CHECK(ckData.size() > 0);
  NDN_LOG_DEBUG("Content data name: " << contentData.at(0)->getName());
  producerFace.setInterestFilter(producerCert.getIdentity(),
    [&] (const ndn::InterestFilter&, const ndn::Interest& interest) {
      NDN_LOG_INFO("Consumer request for " << interest.toUri());
      for (auto seg : contentData) {
        bool exactSeg = interest.getName() == seg->getName();
        bool probeSeg = (interest.getName() == seg->getName().getPrefix(-1)) &&
                         interest.getCanBePrefix();
        if (exactSeg || probeSeg) {
          producerFace.put(*seg);
          break;
        }
      }
      if (interest.getName().isPrefixOf(ckData.at(0)->getName())) {
        producerFace.put(*ckData.at(0));
      }
    }
  );

  bool isConsumeCbCalled = false;
  consumer1.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer1.consume(producerCert.getIdentity().append(dataName),
    [&] (const Buffer& result) {
      isConsumeCbCalled = true;
      BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(),
                                    PLAIN_TEXT, PLAIN_TEXT + sizeof(PLAIN_TEXT));
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);

  ndn::nacabe::algo::ABESupport::getInstance().clearCachedContentKeys();
  isConsumeCbCalled = false;
  consumer2.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer2.consume(producerCert.getIdentity().append(dataName),
    [] (const Buffer&) {
      BOOST_CHECK(false);
    },
    [&] (const std::string&) {
      isConsumeCbCalled = true;
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);
}

BOOST_AUTO_TEST_CASE(Kp)
{
  // set up AA
  NDN_LOG_INFO("Create Attribute Authority. AA prefix: " << aaCert.getIdentity());
  security::ValidatorConfig validator(aaFace);
  validator.load("trust-schema.conf");
  KpAttributeAuthority aa(aaCert, aaFace, validator, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  // define attr list for consumer rights
  Policy policy1 = "cs";
  NDN_LOG_INFO("Add comsumer 1 "<< consumerCert1.getIdentity() <<" with policy: " << policy1);
  aa.addNewPolicy(consumerCert1, policy1);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 1);

  Policy policy2 = "cs and homework";
  NDN_LOG_INFO("Add comsumer 2 "<<consumerCert2.getIdentity()<<" with policy: " << policy2);
  aa.addNewPolicy(consumerCert2, policy2);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 2);

  // set up consumer
  NDN_LOG_INFO("Create Consumer 1. Consumer 1 prefix:"<<consumerCert1.getIdentity());
  security::ValidatorConfig validator1(consumerFace1);
  validator1.load("trust-schema.conf");
  Consumer consumer1(consumerFace1, m_keyChain, validator1, consumerCert1, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer1.m_paramFetcher.getPublicParams().m_pub != "");

  // set up consumer
  NDN_LOG_INFO("Create Consumer 2. Consumer 2 prefix:"<<consumerCert2.getIdentity());
  security::ValidatorConfig validator2(consumerFace2);
  validator2.load("trust-schema.conf");
  Consumer consumer2(consumerFace2, m_keyChain, validator2, consumerCert2, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(consumer2.m_paramFetcher.getPublicParams().m_pub != "");

  // set up producer
  NDN_LOG_INFO("Create Producer. Producer prefix:"<<producerCert.getIdentity());
  security::ValidatorConfig validator3(producerFace);
  validator3.load("trust-schema.conf");
  Producer producer(producerFace, m_keyChain, validator3, producerCert, aaCert, dataOwnerCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(producer.m_paramFetcher.getPublicParams().m_pub != "");

  // set up data owner
  NDN_LOG_INFO("Create Data Owner. Data Owner prefix:"<<dataOwnerCert.getIdentity());
  DataOwner dataOwner(dataOwnerCert, dataOwnerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  NDN_LOG_INFO("\n=================== start work flow ==================\n");

  Name dataName = "/dataName";
  std::vector<std::string> attr = {"cs", "exam"};

  bool isPolicySet = false;
  dataOwner.commandProducerPolicy(producerCert.getIdentity(), dataName, attr,
    [&] (const Data& response) {
      NDN_LOG_DEBUG("on policy set data callback");
      isPolicySet = true;
      BOOST_CHECK_EQUAL(readString(response.getContent()), "success");
      auto attrFound = producer.findMatchedAttributes(dataName);
      BOOST_CHECK_EQUAL_COLLECTIONS(attrFound.begin(), attrFound.end(), attr.begin(), attr.end());
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );

  NDN_LOG_DEBUG("Before policy set");
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isPolicySet);

  SPtrVector<ndn::Data> contentData, ckData;
  auto attributeFound = producer.findMatchedAttributes(dataName);
  std::tie(contentData, ckData) = producer.produce(dataName, attributeFound, PLAIN_TEXT, signingInfo);
  BOOST_CHECK(contentData.size() > 0);
  BOOST_CHECK(ckData.size() > 0);
  NDN_LOG_DEBUG("Content data name: " << contentData.at(0)->getName());

  producerFace.setInterestFilter(producerCert.getIdentity(),
    [&] (const ndn::InterestFilter&, const ndn::Interest& interest) {
      NDN_LOG_INFO("consumer request for" << interest.toUri());
      if (interest.getName().isPrefixOf(contentData.at(0)->getName())) {
        producerFace.put(*contentData.at(0));
      }
      if (interest.getName().isPrefixOf(ckData.at(0)->getName())) {
        producerFace.put(*ckData.at(0));
      }
    }
  );

  bool isConsumeCbCalled = false;
  consumer1.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer1.consume(producerCert.getIdentity().append(dataName),
    [&] (const Buffer& result) {
      isConsumeCbCalled = true;
      BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(),
                                    PLAIN_TEXT, PLAIN_TEXT + sizeof(PLAIN_TEXT));
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);

  ndn::nacabe::algo::ABESupport::getInstance().clearCachedContentKeys();
  isConsumeCbCalled = false;
  consumer2.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer2.consume(producerCert.getIdentity().append(dataName),
    [] (const Buffer&) {
      BOOST_CHECK(false);
    },
    [&] (const std::string&) {
      isConsumeCbCalled = true;
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);
}

BOOST_AUTO_TEST_CASE(KpCache)
{
  // set up AA
  NDN_LOG_INFO("Create Attribute Authority. AA prefix: " << aaCert.getIdentity());
  security::ValidatorConfig validator(aaFace);
  validator.load("trust-schema.conf");
  KpAttributeAuthority aa(aaCert, aaFace, validator, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  // define attr list for consumer rights
  Policy policy1 = "cs";
  NDN_LOG_INFO("Add comsumer 1 "<< consumerCert1.getIdentity() <<" with policy: " << policy1);
  aa.addNewPolicy(consumerCert1, policy1);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 1);

  Policy policy2 = "cs and homework";
  NDN_LOG_INFO("Add comsumer 2 "<<consumerCert2.getIdentity()<<" with policy: " << policy2);
  aa.addNewPolicy(consumerCert2, policy2);
  BOOST_CHECK_EQUAL(aa.m_tokens.size(), 2);

  // set up consumer
  NDN_LOG_INFO("Create Consumer 1. Consumer 1 prefix:"<<consumerCert1.getIdentity());
  security::ValidatorConfig validator1(consumerFace1);
  validator1.load("trust-schema.conf");
  Consumer consumer1(consumerFace1, m_keyChain, validator1, consumerCert1, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  BOOST_CHECK(consumer1.m_paramFetcher.getPublicParams().m_pub != "");

  // set up consumer
  NDN_LOG_INFO("Create Consumer 2. Consumer 2 prefix:"<<consumerCert2.getIdentity());
  security::ValidatorConfig validator2(consumerFace2);
  validator2.load("trust-schema.conf");
  Consumer consumer2(consumerFace2, m_keyChain, validator2, consumerCert2, aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  BOOST_CHECK(consumer2.m_paramFetcher.getPublicParams().m_pub != "");

  // set up producer
  NDN_LOG_INFO("Create Producer. Producer prefix:"<<producerCert.getIdentity());
  security::ValidatorConfig validator3(producerFace);
  validator3.load("trust-schema.conf");
  CacheProducer producer(producerFace, m_keyChain, validator3, producerCert, aaCert, dataOwnerCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(anchorCert);
  BOOST_CHECK(producer.m_paramFetcher.getPublicParams().m_pub != "");

  // set up data owner
  NDN_LOG_INFO("Create Data Owner. Data Owner prefix:"<<dataOwnerCert.getIdentity());
  DataOwner dataOwner(dataOwnerCert, dataOwnerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  NDN_LOG_INFO("\n=================== start work flow ==================\n");

  Name dataName = "/dataName";
  std::vector<std::string> attr = {"cs", "exam"};

  bool isPolicySet = false;
  dataOwner.commandProducerPolicy(producerCert.getIdentity(), dataName, attr,
    [&] (const Data& response) {
      NDN_LOG_DEBUG("on policy set data callback");
      isPolicySet = true;
      BOOST_CHECK_EQUAL(readString(response.getContent()), "success");
      auto attrFound = producer.findMatchedAttributes(dataName);
      BOOST_CHECK_EQUAL_COLLECTIONS(attrFound.begin(), attrFound.end(), attr.begin(), attr.end());
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );

  NDN_LOG_DEBUG("Before policy set");
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isPolicySet);
  SPtrVector<ndn::Data>  contentData, ckData;
  auto attributeFound = producer.findMatchedAttributes(dataName);
  BOOST_CHECK(producer.m_kpKeyCache.size() == 0);
  std::tie(contentData, ckData) = producer.produce(dataName, attributeFound, PLAIN_TEXT, signingInfo);
  BOOST_CHECK(producer.m_kpKeyCache.size() == 1);
  std::tie(contentData, ckData) = producer.produce(dataName, attributeFound, PLAIN_TEXT, signingInfo);
  BOOST_CHECK(producer.m_kpKeyCache.size() == 1);
  BOOST_CHECK(contentData.size() > 0);
  BOOST_CHECK(ckData.size() > 0);
  NDN_LOG_DEBUG("Content data name: " << contentData.at(0)->getName());

  producerFace.setInterestFilter(producerCert.getIdentity(),
    [&] (const ndn::InterestFilter&, const ndn::Interest& interest) {
      NDN_LOG_INFO("consumer request for" << interest.toUri());
      if (interest.getName().isPrefixOf(contentData.at(0)->getName())) {
        producerFace.put(*contentData.at(0));
      }
      if (interest.getName().isPrefixOf(ckData.at(0)->getName())) {
        producerFace.put(*ckData.at(0));
      }
    }
  );

  bool isConsumeCbCalled = false;
  consumer1.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer1.consume(producerCert.getIdentity().append(dataName),
    [&] (const Buffer& result) {
      isConsumeCbCalled = true;
      BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(),
                                    PLAIN_TEXT, PLAIN_TEXT + sizeof(PLAIN_TEXT));
    },
    [] (const std::string&) {
      BOOST_CHECK(false);
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);

  ndn::nacabe::algo::ABESupport::getInstance().clearCachedContentKeys();
  isConsumeCbCalled = false;
  consumer2.obtainDecryptionKey();
  advanceClocks(time::milliseconds(20), 60);
  consumer2.consume(producerCert.getIdentity().append(dataName),
    [] (const Buffer&) {
      BOOST_CHECK(false);
    },
    [&] (const std::string&) {
      isConsumeCbCalled = true;
    }
  );
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace2.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);
  BOOST_CHECK(isConsumeCbCalled);
}

BOOST_AUTO_TEST_CASE(KpCacheProducerEncryptDecryptBenchmark1000_ExistingPolicy)
{
  constexpr int N = 1000;

  auto buffersEqual = [](const Buffer& b, const uint8_t* ref, size_t refLen) -> bool {
    return b.size() == refLen && std::memcmp(b.data(), ref, refLen) == 0;
  };

  // =========================================================
  // 1) Setup AA (KP-ABE) -- exactly like old integrated test
  // =========================================================
  security::ValidatorConfig validatorAA(aaFace);
  validatorAA.load("trust-schema.conf");
  KpAttributeAuthority aa(aaCert, aaFace, validatorAA, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  Policy policyForConsumer1 = "cs and homework"; // existing policy
  aa.addNewPolicy(consumerCert1, policyForConsumer1);
  BOOST_REQUIRE_EQUAL(aa.m_tokens.size(), 1);

  Policy policyForConsumer2 = "cs";
  aa.addNewPolicy(consumerCert2, policyForConsumer2);
  BOOST_REQUIRE_EQUAL(aa.m_tokens.size(), 2);

  // =========================================================
  // 2) Setup Consumer1 (DO NOT change linkTo topology)
  //    Mimic old test: create, then receive aaCert + anchorCert, then check m_pub.
  // =========================================================
  security::ValidatorConfig validatorC1(consumerFace1);
  validatorC1.load("trust-schema.conf");
  Consumer consumer1(consumerFace1, m_keyChain, validatorC1, consumerCert1, aaCert);

  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);

  BOOST_REQUIRE_MESSAGE(consumer1.m_paramFetcher.getPublicParams().m_pub != "",
                        "Consumer failed to fetch/validate AA public parameters. "
                        "Check trust-schema + cert delivery.");

  // =========================================================
  // 3) Setup CacheProducer (like old test)
  // =========================================================
  security::ValidatorConfig validatorP(producerFace);
  validatorP.load("trust-schema.conf");
  CacheProducer producer(producerFace, m_keyChain, validatorP, producerCert, aaCert, dataOwnerCert);

  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  producerFace.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);

  BOOST_REQUIRE(producer.m_paramFetcher.getPublicParams().m_pub != "");

  // =========================================================
  // 4) Setup DataOwner: set ciphertext attributes (match policy)
  // =========================================================
  DataOwner dataOwner(dataOwnerCert, dataOwnerFace, m_keyChain);
  advanceClocks(time::milliseconds(20), 60);

  const std::vector<std::string> attrs = {"cs", "homework"};
  Name baseDataName = "/dataName";

  bool isPolicySet = false;
  dataOwner.commandProducerPolicy(producerCert.getIdentity(), baseDataName, attrs,
    [&] (const Data& response) {
      isPolicySet = true;
      BOOST_REQUIRE_EQUAL(readString(response.getContent()), "success");
    },
    [] (const std::string&) {
      BOOST_FAIL("commandProducerPolicy failed");
    }
  );

  advanceClocks(time::milliseconds(20), 60);
  BOOST_REQUIRE(isPolicySet);

  auto attributeFound = producer.findMatchedAttributes(baseDataName);
  BOOST_REQUIRE(!attributeFound.empty());

  // =========================================================
  // 5) Encrypt benchmark: produce N unique names + build lookup tables
  // =========================================================
  std::unordered_map<std::string, std::shared_ptr<ndn::Data>> exactDataByName;
  std::unordered_map<std::string, std::shared_ptr<ndn::Data>> probeDataByPrefix;

  // warmup (not counted)
  {
    SPtrVector<ndn::Data> wContent, wCk;
    std::tie(wContent, wCk) = producer.produce(Name(baseDataName).append("warmup"),
                                               attributeFound, PLAIN_TEXT, signingInfo);
    BOOST_REQUIRE(!wContent.empty());
    BOOST_REQUIRE(!wCk.empty());
  }

  long long encTotalUs = 0;
  size_t encSink = 0;

  for (int i = 0; i < N; ++i) {
    Name dataName = Name(baseDataName).appendNumber(static_cast<uint64_t>(i));

    auto t0 = std::chrono::steady_clock::now();

    SPtrVector<ndn::Data> contentData, ckData;
    std::tie(contentData, ckData) = producer.produce(dataName, attributeFound, PLAIN_TEXT, signingInfo);

    auto t1 = std::chrono::steady_clock::now();
    encTotalUs += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    BOOST_REQUIRE(!contentData.empty());
    BOOST_REQUIRE(!ckData.empty());
    encSink += contentData.size() + ckData.size();

    for (auto& seg : contentData) {
      exactDataByName.emplace(seg->getName().toUri(), seg);
      probeDataByPrefix.emplace(seg->getName().getPrefix(-1).toUri(), seg);
    }
    exactDataByName.emplace(ckData.at(0)->getName().toUri(), ckData.at(0));
    probeDataByPrefix.emplace(ckData.at(0)->getName().getPrefix(-1).toUri(), ckData.at(0));
  }

  const double encAvgUs = encTotalUs / static_cast<double>(N);
  BOOST_TEST_MESSAGE("KP-ABE Encrypt (CacheProducer::produce, attrs={cs,homework}) avg over "
                     << N << " runs: " << encAvgUs << " us (sink=" << encSink << ")");
  BOOST_TEST_MESSAGE("CacheProducer kpKeyCache size after encrypt bench: " << producer.m_kpKeyCache.size());

  // =========================================================
  // 6) Producer InterestFilter: serve certificates + content/ck
  //    IMPORTANT: also serve AA cert fetch, not just producer cert fetch
  // =========================================================
  const ndn::Name producerKeyPrefix = producerCert.getIdentity().append("KEY");
  const ndn::Name aaKeyPrefix       = aaCert.getIdentity().append("KEY");
  const ndn::Name anchorKeyPrefix   = anchorCert.getIdentity().append("KEY");

  producerFace.setInterestFilter(Name("/"),
    [&] (const ndn::InterestFilter&, const ndn::Interest& interest) {
      const ndn::Name& in = interest.getName();

      // Serve cert chain requests for producer / AA / anchor
      if (producerKeyPrefix.isPrefixOf(in)) {
        producerFace.put(producerCert);
        return;
      }
      if (aaKeyPrefix.isPrefixOf(in)) {
        producerFace.put(aaCert);
        return;
      }
      if (anchorKeyPrefix.isPrefixOf(in)) {
        producerFace.put(anchorCert);
        return;
      }

      // Serve content/ck
      const std::string inUri = in.toUri();

      auto itExact = exactDataByName.find(inUri);
      if (itExact != exactDataByName.end()) {
        producerFace.put(*itExact->second);
        return;
      }

      if (interest.getCanBePrefix()) {
        auto itProbe = probeDataByPrefix.find(inUri);
        if (itProbe != probeDataByPrefix.end()) {
          producerFace.put(*itProbe->second);
          return;
        }
      }

      // fallback prefix scan (rare)
      for (const auto& kv : exactDataByName) {
        const std::string& dataNameUri = kv.first;
        if (dataNameUri.size() >= inUri.size() && dataNameUri.compare(0, inUri.size(), inUri) == 0) {
          producerFace.put(*kv.second);
          return;
        }
      }
    }
  );

  // Also pre-feed certs (old style)
  consumerFace1.receive(producerCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(aaCert);
  advanceClocks(time::milliseconds(20), 60);
  consumerFace1.receive(anchorCert);
  advanceClocks(time::milliseconds(20), 60);

  // =========================================================
  // 7) Obtain DKEY (key fetch), with bounded pumping
  // =========================================================
  consumer1.obtainDecryptionKey();
  for (int i = 0; i < 10; ++i) {
    advanceClocks(time::milliseconds(20), 60);
  }

  // =========================================================
  // 8) Decrypt benchmark: bounded pumps, progress prints
  // =========================================================
  long long decTotalUs = 0;
  int successCount = 0;
  int failCount = 0;

  auto consumeOnceUs = [&](const Name& dataName, bool verify) -> long long {
    // ndn::nacabe::algo::ABESupport::getInstance().clearCachedContentKeys();

    bool called = false;
    bool ok = false;
    bool verified = true;
    std::string errMsg;

    auto t0 = std::chrono::steady_clock::now();

    consumer1.consume(producerCert.getIdentity().append(dataName),
      [&] (const Buffer& result) {
        called = true;
        ok = true;
        if (verify) {
          verified = buffersEqual(result, PLAIN_TEXT, sizeof(PLAIN_TEXT));
        }
      },
      [&] (const std::string& err) {
        called = true;
        ok = false;
        errMsg = err;
      });

    // bounded pumps
    for (int spin = 0; spin < 12 && !called; ++spin) {
      advanceClocks(time::milliseconds(1), 1);
    }

    auto t1 = std::chrono::steady_clock::now();

    if (!called) {
      ++failCount;
      BOOST_TEST_MESSAGE("consume timeout name=" << dataName.toUri());
      return 0;
    }
    if (!ok) {
      ++failCount;
      BOOST_TEST_MESSAGE("consume error: " << errMsg);
      return 0;
    }
    if (verify && !verified) {
      ++failCount;
      BOOST_TEST_MESSAGE("plaintext mismatch name=" << dataName.toUri());
      return 0;
    }

    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  };

  // warmup decrypt a few
  for (int i = 0; i < 5; ++i) {
    Name dn = Name(baseDataName).appendNumber(static_cast<uint64_t>(i));
    (void)consumeOnceUs(dn, true);
  }

  for (int i = 0; i < N; ++i) {
    if (i % 100 == 0) {
      BOOST_TEST_MESSAGE("decrypt progress: " << i << "/" << N);
    }
    Name dn = Name(baseDataName).appendNumber(static_cast<uint64_t>(i));
    const bool verify = (i < 3) || (i % 200 == 0) || (i == N - 1);

    auto us = consumeOnceUs(dn, verify);
    if (us > 0) {
      decTotalUs += us;
      ++successCount;
    }
  }

  BOOST_TEST_MESSAGE("decrypt successCount=" << successCount << " failCount=" << failCount);
  BOOST_CHECK_EQUAL(failCount, 0);

  if (successCount > 0) {
    const double decAvgUs = decTotalUs / static_cast<double>(successCount);
    BOOST_TEST_MESSAGE("KP-ABE Decrypt avg over " << successCount << " successful runs: "
                       << decAvgUs << " us");
  }
}



BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace nacabe
} // namespace ndn
